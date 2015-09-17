/**********************************************************************
File name: terraform.cpp
This file is part of: SCC (working title)

LICENSE

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

FEEDBACK & QUESTIONS

For feedback and questions about SCC please e-mail one of the authors named in
the AUTHORS file.
**********************************************************************/
#include "terraform.hpp"
#include "ui_terraform.h"

#include <QMouseEvent>

#include "ffengine/io/filesystem.hpp"

#include "ffengine/math/intersect.hpp"
#include "ffengine/math/perlin.hpp"
#include "ffengine/math/algo.hpp"

#include "ffengine/sim/world_ops.hpp"

#include "ffengine/render/grid.hpp"
#include "ffengine/render/pointer.hpp"
#include "ffengine/render/fluid.hpp"
#include "ffengine/render/aabb.hpp"
#include "ffengine/render/oct_sphere.hpp"
#include "ffengine/render/curve.hpp"

#include "application.hpp"

static io::Logger &logger = io::logging().get_logger("app.terraform");

#include <chrono>
typedef std::chrono::steady_clock timelog_clock;
#define ms_cast(x) std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1000> > >(x)

static const char *brush_search_paths[] = {
    ":/brushes/",
    "/usr/share/gimp/2.0/brushes/",
    "/usr/share/kde4/apps/krita/brushes/"
};

void load_image_to_texture(const QString &url)
{
    QImage texture = QImage(url);
    texture = texture.convertToFormat(QImage::Format_ARGB32);

    uint8_t *pixbase = texture.bits();
    for (int i = 0; i < texture.width()*texture.height(); i++)
    {
        const uint8_t A = pixbase[3];
        const uint8_t R = pixbase[2];
        const uint8_t G = pixbase[1];
        const uint8_t B = pixbase[0];

        pixbase[0] = R;
        pixbase[1] = G;
        pixbase[2] = B;
        pixbase[3] = A;

        pixbase += 4;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0,
                    texture.width(), texture.height(),
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    texture.bits());
    glGenerateMipmap(GL_TEXTURE_2D);
}


std::ostream &operator<<(std::ostream &stream, const QModelIndex &index)
{
    if (index.isValid()) {
        return stream << "QModelIndex(" << index.row() << ", " << index.column() << ")";
    }
    return stream << "QModelIndex()";
}


TerraformScene::TerraformScene(
        ffe::FancyTerrainInterface &terrain_interface,
        const sim::WorldState &state,
        QSize window_size,
        ffe::DynamicAABBs::DiscoverCallback &&aabb_callback):
    m_scene(m_scenegraph, m_camera),
    m_rendergraph(m_scene),
    m_prewater_buffer(m_resources.emplace<ffe::FBO>(
                          "prewater_buffer",
                          window_size.width(),
                          window_size.height())),
    m_prewater_colour(m_resources.emplace<ffe::Texture2D>(
                          "prewater_buffer/colour",
                          GL_RGBA8, window_size.width(), window_size.height())),
    m_prewater_depth(m_resources.emplace<ffe::Texture2D>(
                         "prewater_buffer/depth",
                         GL_DEPTH_COMPONENT24,
                         window_size.width(), window_size.height(),
                         GL_DEPTH_COMPONENT, GL_FLOAT)),
    m_solid_pass(m_rendergraph.new_node<ffe::RenderPass>(m_prewater_buffer)),
    m_transparent_pass(m_rendergraph.new_node<ffe::RenderPass>(m_prewater_buffer)),
    m_water_pass(m_rendergraph.new_node<ffe::RenderPass>(m_window)),
    m_grass(load_texture_resource("grass", ":/textures/grass00.png")),
    m_rock(load_texture_resource("rock", ":/textures/rock00.png")),
    m_blend(load_texture_resource("blend", ":/textures/blend00.png")),
    m_waves(load_texture_resource("waves", ":/textures/waves00.png")),
    m_full_terrain(m_scenegraph.root().emplace<ffe::FullTerrainNode>(
                       terrain_interface.size(),
                       terrain_interface.grid_size())),
    m_terrain_geometry(m_full_terrain.emplace<ffe::FancyTerrainNode>(
                           terrain_interface, m_resources, m_solid_pass)),
    m_drag_plane_vbo(ffe::VBOFormat({ffe::VBOAttribute(3), ffe::VBOAttribute(3)})),
    m_fancy_drag_plane_material(m_resources.emplace<ffe::Material>(
                                   "test_drag_plane",
                                   m_drag_plane_vbo,
                                   m_drag_plane_ibo)),
    m_overlay_material(m_resources.manage<ffe::Material>(
                           "brush_overlay",
                           std::move(ffe::Material::shared_with(m_terrain_geometry.material())))),
    m_bezier_material(m_resources.emplace<ffe::Material>(
                          "bezier",
                          ffe::VBOFormat({ffe::VBOAttribute(4)}))),
    m_road_material(m_resources.emplace<ffe::Material>(
                        "road",
                        ffe::VBOFormat({ffe::VBOAttribute(4)}))),
    m_aabb_material(m_resources.emplace<ffe::Material>(
                        "debug/aabb",
                        ffe::VBOFormat({ffe::VBOAttribute(4)}))),
    m_fluid_source_material(m_resources.emplace<ffe::FluidSourceMaterial>("terraform/fluidsource", 13)),
    m_brush(m_resources.emplace<ffe::Texture2D>(
                "runtime/brush_texture",
                GL_R32F, 129, 129,
                GL_RED,
                GL_FLOAT)),
    m_octree_group(m_scenegraph.root().emplace<ffe::scenegraph::OctreeGroup>())
{
    m_scenegraph.set_sun_colour(Vector4f(1, 0.99, 0.95, 2.3));
    m_scenegraph.set_sun_direction(Vector3f(1, 1, 4).normalized());
    m_scenegraph.set_sky_colour(Vector4f(0.95, 0.99, 1.0, 1.0));

    m_prewater_colour.bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m_prewater_depth.bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m_solid_pass.set_clear_mask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    m_solid_pass.set_clear_colour(Vector4f(0.5, 0.4, 0.3, 1.0));

    m_transparent_pass.set_clear_mask(0);
    m_transparent_pass.dependencies().push_back(&m_solid_pass);

    m_water_pass.set_blit_colour_src(&m_prewater_buffer);
    m_water_pass.set_blit_depth_src(&m_prewater_buffer);
    m_water_pass.dependencies().push_back(&m_transparent_pass);

    if (!m_rendergraph.resort()) {
        throw std::runtime_error("rendergraph has cycles");
    }

    m_camera.controller().set_distance(40.0);
    m_camera.controller().set_rot(Vector2f(-60.f / 180.f * M_PI, 0));
    m_camera.controller().set_pos(Vector3f(20, 30, 20));
    m_camera.set_fovy(60. / 180. * M_PI);
    m_camera.set_zfar(10000);
    m_camera.set_znear(1);

    /* terrain configuration */

    m_full_terrain.set_detail_level(0);

    m_terrain_geometry.set_enable_linear_filter(false);
    m_terrain_geometry.attach_grass_texture(&m_grass);
    m_terrain_geometry.attach_rock_texture(&m_rock);
    m_terrain_geometry.attach_blend_texture(&m_blend);

    {
        spp::EvaluationContext ctx(m_resources.shader_library());
        ffe::MaterialPass &pass = m_overlay_material.make_pass_material(m_solid_pass);
        bool success = pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/terrain/brush_overlay.frag"),
                    ctx,
                    GL_FRAGMENT_SHADER);

        if (!success || !m_terrain_geometry.configure_overlay_material(m_overlay_material)) {
            throw std::runtime_error("overlay material failed to build");
        }

        m_brush.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    m_overlay_material.attach_texture("brush", &m_brush);

    /* fluid configuration */

    m_full_terrain.emplace<ffe::CPUFluid>(m_resources,
                                          state.fluid(),
                                          m_transparent_pass,
                                          m_water_pass);

    /* drag plane materials */

    {
        spp::EvaluationContext ctx(m_resources.shader_library());
        ffe::MaterialPass &pass = m_fancy_drag_plane_material.make_pass_material(m_water_pass);
        bool success = true;

        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/terraform/drag_plane.vert"),
                    ctx,
                    GL_VERTEX_SHADER);
        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/terraform/fancy_drag_plane.frag"),
                    ctx,
                    GL_FRAGMENT_SHADER);

        m_fancy_drag_plane_material.declare_attribute("position", 0);
        m_fancy_drag_plane_material.declare_attribute("normal", 1);

        success = success && m_fancy_drag_plane_material.link();

        if (!success) {
            throw std::runtime_error("failed to compile or link AABB material");
        }

        m_fancy_drag_plane_material.attach_texture("scene_depth", &m_prewater_depth);
        m_fancy_drag_plane_material.attach_texture("scene_colour", &m_prewater_colour);
    }

    /* testing materials */

    {
        spp::EvaluationContext ctx(m_resources.shader_library());
        ffe::MaterialPass &pass = m_aabb_material.make_pass_material(m_solid_pass);
        bool success = true;

        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/aabb/main.vert"),
                    ctx,
                    GL_VERTEX_SHADER);
        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/aabb/main.frag"),
                    ctx,
                    GL_FRAGMENT_SHADER);

        m_aabb_material.declare_attribute("position_t", 0);

        success = success && m_aabb_material.link();

        if (!success) {
            throw std::runtime_error("failed to compile or link AABB material");
        }
    }

    m_scenegraph.root().emplace<ffe::DynamicAABBs>(
                m_aabb_material,
                std::move(aabb_callback));

    {
        spp::EvaluationContext ctx(m_resources.shader_library());
        ffe::MaterialPass &pass = m_bezier_material.make_pass_material(m_solid_pass);
        bool success = true;

        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/curve/main.vert"),
                    ctx,
                    GL_VERTEX_SHADER);
        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/curve/main.frag"),
                    ctx,
                    GL_FRAGMENT_SHADER);

        m_bezier_material.declare_attribute("position_t", 0);

        success = success && m_bezier_material.link();

        if (!success) {
            throw std::runtime_error("failed to compile or link bezier material");
        }
    }

    {
        spp::EvaluationContext ctx(m_resources.shader_library());
        ffe::MaterialPass &pass = m_road_material.make_pass_material(m_solid_pass);
        bool success = true;

        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/curve/main.vert"),
                    ctx,
                    GL_VERTEX_SHADER);
        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/curve/main.frag"),
                    ctx,
                    GL_FRAGMENT_SHADER);

        m_road_material.declare_attribute("position_t", 0);

        success = success && m_road_material.link();

        if (!success) {
            throw std::runtime_error("failed to compile or link bezier material");
        }

        m_road_material.set_polygon_mode(GL_LINE);
    }

    {
        spp::EvaluationContext ctx(m_resources.shader_library());
        ffe::MaterialPass &pass = m_fluid_source_material.make_pass_material(m_transparent_pass);
        bool success = true;

        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/terraform/fluidsource.vert"),
                    ctx);

        success = success && pass.shader().attach(
                    m_resources.load_shader_checked(":/shaders/terraform/fluidsource.frag"),
                    ctx);

        success = success && m_fluid_source_material.link();

        if (!success) {
            throw std::runtime_error("failed to compile or link fluid source material");
        }
    }
}

TerraformScene::~TerraformScene()
{

}

ffe::Texture2D &TerraformScene::load_texture_resource(
        const std::string &resource_name,
        const QString &source_path)
{
    QImage texture_image = QImage(source_path);
    texture_image = texture_image.convertToFormat(QImage::Format_ARGB32);

    ffe::Texture2D &tex = m_resources.emplace<ffe::Texture2D>(
                resource_name,
                GL_RGBA,
                texture_image.width(), texture_image.height());
    tex.bind();

    uint8_t *pixbase = texture_image.bits();
    for (int i = 0; i < texture_image.width()*texture_image.height(); i++)
    {
        const uint8_t A = pixbase[3];
        const uint8_t R = pixbase[2];
        const uint8_t G = pixbase[1];
        const uint8_t B = pixbase[0];

        pixbase[0] = R;
        pixbase[1] = G;
        pixbase[2] = B;
        pixbase[3] = A;

        pixbase += 4;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0,
                    texture_image.width(), texture_image.height(),
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    texture_image.bits());
    glGenerateMipmap(GL_TEXTURE_2D);

    return tex;
}

void TerraformScene::update_size(const QSize &new_size)
{
    if (m_window.width() != new_size.width() ||
            m_window.height() != new_size.height() )
    {
        // resize window
        m_window.set_size(new_size.width(), new_size.height());
        // resize FBO
        m_prewater_buffer = ffe::FBO(new_size.width(), new_size.height());
        m_prewater_colour.bind();
        m_prewater_colour.reinit(GL_RGBA, new_size.width(), new_size.height());
        m_prewater_depth.bind();
        m_prewater_depth.reinit(GL_DEPTH_COMPONENT24, new_size.width(), new_size.height());
        m_prewater_buffer.attach(GL_COLOR_ATTACHMENT0, &m_prewater_colour);
        m_prewater_buffer.attach(GL_DEPTH_ATTACHMENT, &m_prewater_depth);
    }
}


BrushWrapper::BrushWrapper(std::unique_ptr<Brush> &&brush,
                           const QString &display_name):
    m_brush(std::move(brush)),
    m_display_name(display_name),
    m_preview_icon(m_brush->preview_image(preview_size))
{

}

BrushWrapper::~BrushWrapper()
{

}


BrushList::BrushList(QObject *parent):
    QAbstractListModel(parent)
{

}

BrushWrapper *BrushList::resolve_index(const QModelIndex &index)
{
    if (!index.isValid()) {
        return nullptr;
    }
    return m_brushes[index.row()].get();
}

const BrushWrapper *BrushList::resolve_index(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    return m_brushes[index.row()].get();
}

QVariant BrushList::data(const QModelIndex &index, int role = Qt::DisplayRole) const
{
    const BrushWrapper *brush = resolve_index(index);
    if (!brush) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    {
        return brush->m_display_name;
    }
    case Qt::DecorationRole:
    {
        return brush->m_preview_icon;
    }
    default: return QVariant();
    }
}

Qt::ItemFlags BrushList::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return 0;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int BrushList::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    int result = m_brushes.size();
    return result;
}

bool BrushList::setData(const QModelIndex &index, const QVariant &value, int role)
{
    BrushWrapper *brush = resolve_index(index);
    if (!brush) {
        return false;
    }

    switch (role)
    {
    case Qt::DisplayRole:
    {
        if (value.canConvert<QString>()) {
            brush->m_display_name = value.toString();

            QVector<int> roles;
            roles.append(role);
            dataChanged(index, index, roles);
            return true;
        }
        break;
    }
    }
    return false;
}

void BrushList::append(std::unique_ptr<Brush> &&brush, const QString &display_name)
{
    beginInsertRows(QModelIndex(), m_brushes.size(), m_brushes.size());
    m_brushes.emplace_back(new BrushWrapper(std::move(brush), display_name));
    endInsertRows();
}

void BrushList::append(const gamedata::PixelBrushDef &brush)
{
    append(std::make_unique<ImageBrush>(brush),
           QString::fromStdString(brush.display_name()));
}


/* CameraDrag */

CameraDrag::CameraDrag(ffe::PerspectivalCamera &camera,
                       const Vector2f &viewport_size,
                       const Vector3f &start_world_pos):
    PlaneToolDrag(Plane(start_world_pos, Vector3f(0, 0, 1)),
                  camera,
                  viewport_size,
                  std::bind(&CameraDrag::plane_drag,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2)),
    m_camera(camera),
    m_drag_point(start_world_pos),
    m_drag_camera_pos(camera.controller().pos())
{

}

sim::WorldOperationPtr CameraDrag::plane_drag(const Vector2f &,
                                              const Vector3f &world_pos)
{
    const Vector3f moved = m_drag_point - world_pos;
    m_camera.controller().set_pos(m_drag_camera_pos + moved);
    m_drag_camera_pos = m_camera.controller().pos();
    return nullptr;
}


/* TerraformMode */

TerraformMode::TerraformMode(Application &app, QWidget *parent):
    ApplicationMode(app, parent),
    m_ui(new Ui::TerraformMode),
    m_tools(nullptr),
    m_server(nullptr),
    m_terrain_interface(nullptr),
    m_t(100),
    m_mouse_mode(MOUSE_IDLE),
    m_mouse_action(nullptr),
    m_drag(nullptr),
    m_brush_changed(true),
    m_curr_tool(nullptr),
    m_brush_objects(this),
    m_paused(false)
{
    m_ui->setupUi(this);
    setMouseTracking(true);

    {
        std::string group_name(QT_TRANSLATE_NOOP("Bindings", "Map editor: Terraforming"));
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_terrain_level);
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_terrain_raise_lower);
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_terrain_ramp);
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_terrain_smooth);
    }
    {
        std::string group_name(QT_TRANSLATE_NOOP("Bindings", "Map editor: Fluid"));
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_fluid_edit_sources);
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_fluid_ocean_level);
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_fluid_raise_lower);
    }
    {
        std::string group_name(QT_TRANSLATE_NOOP("Bindings", "Map editor: Testing"));
        m_app.keybindings().add_item(group_name, m_ui->action_terraform_tool_test);
    }

    m_ui->toolbtn_terrain_raise_lower->setDefaultAction(m_ui->action_terraform_tool_terrain_raise_lower);
    m_ui->toolbtn_terrain_flatten->setDefaultAction(m_ui->action_terraform_tool_terrain_level);
    m_ui->toolbtn_terrain_smooth->setDefaultAction(m_ui->action_terraform_tool_terrain_smooth);
    m_ui->toolbtn_terrain_ramp->setDefaultAction(m_ui->action_terraform_tool_terrain_ramp);
    m_ui->toolbtn_fluid_raise_lower->setDefaultAction(m_ui->action_terraform_tool_fluid_raise_lower);
    m_ui->toolbtn_testing->setDefaultAction(m_ui->action_terraform_tool_test);
    m_ui->toolbtn_fluid_source->setDefaultAction(m_ui->action_terraform_tool_fluid_edit_sources);

    m_tools.addAction(m_ui->action_terraform_tool_fluid_edit_sources);
    m_tools.addAction(m_ui->action_terraform_tool_fluid_raise_lower);
    m_tools.addAction(m_ui->action_terraform_tool_test);
    m_tools.addAction(m_ui->action_terraform_tool_terrain_ramp);
    m_tools.addAction(m_ui->action_terraform_tool_terrain_smooth);
    m_tools.addAction(m_ui->action_terraform_tool_terrain_level);
    m_tools.addAction(m_ui->action_terraform_tool_terrain_raise_lower);

    m_ui->tabWidget->tabBar()->setDrawBase(false);

    m_ui->brush_list->setModel(&m_brush_objects);

    setFocusPolicy(Qt::StrongFocus);

    m_brush_objects.append(std::make_unique<ParzenBrush>(), "Parzen");
    m_brush_objects.append(std::make_unique<CircleBrush>(), "Circle");

    m_brush_frontend.set_brush(m_brush_objects.vector()[0]->m_brush.get());
    m_ui->slider_brush_size->setValue(64);
    m_ui->slider_brush_strength->setValue(m_ui->slider_brush_strength->maximum());

    addAction(m_app.shared_actions().action_camera_pan);
    addAction(m_app.shared_actions().action_camera_rotate);
    addAction(m_app.shared_actions().action_camera_zoom);
    addAction(m_app.shared_actions().action_tool_primary);
    addAction(m_app.shared_actions().action_tool_secondary);

    m_mouse_dispatcher.actions().emplace_back(m_app.shared_actions().action_camera_pan);
    m_mouse_dispatcher.actions().emplace_back(m_app.shared_actions().action_camera_rotate);
    m_mouse_dispatcher.actions().emplace_back(m_app.shared_actions().action_camera_zoom);
    m_mouse_dispatcher.actions().emplace_back(m_app.shared_actions().action_tool_primary);
    m_mouse_dispatcher.actions().emplace_back(m_app.shared_actions().action_tool_secondary);

    connect(m_app.shared_actions().action_camera_pan, &MouseAction::triggered,
            this, &TerraformMode::on_camera_pan_triggered);
    connect(m_app.shared_actions().action_camera_rotate, &MouseAction::triggered,
            this, &TerraformMode::on_camera_rotate_triggered);
    /*connect(m_app.shared_actions().action_camera_zoom, &MouseAction::triggered,
            this, &TerraformMode::on_camera_zoom_triggered);*/

    connect(m_app.shared_actions().action_tool_primary, &MouseAction::triggered,
            this, &TerraformMode::on_tool_primary_triggered);
    connect(m_app.shared_actions().action_tool_secondary, &MouseAction::triggered,
            this, &TerraformMode::on_tool_secondary_triggered);
}

TerraformMode::~TerraformMode()
{
    delete m_ui;
}

void TerraformMode::keyPressEvent(QKeyEvent *event)
{
    if (m_mouse_action != nullptr) {
        if (event->key() == Qt::Key_Escape)
        {
            clear_mouse_mode();
        }
        return;
    }
}

void TerraformMode::advance(ffe::TimeInterval dt)
{
    if (m_scene) {
        m_scene->m_camera.advance(dt);
        m_scene->m_scenegraph.advance(dt);
    }
    if (m_mouse_mode == MOUSE_TOOL_DRAG && m_drag->is_continuous()) {
        sim::WorldOperationPtr op(m_drag->drag(m_mouse_pos_win));
        if (op) {
            m_server->enqueue_op(std::move(op));
        }
    }
    /*if (dt > 0.02) {
        logger.logf(io::LOG_WARNING, "long frame: %.4f seconds", dt);
    }*/

}

void TerraformMode::after_gl_sync()
{
    m_sync_lock.unlock();
    if (m_scene) {
        m_ui->ldebug_octree_selected_objects->setNum((int)m_scene->m_octree_group.selected_objects());
        m_ui->ldebug_fps->setNum(std::round(m_gl_scene->fps()));
    }
}

void TerraformMode::before_gl_sync()
{
    ffe::raise_last_gl_error();
    prepare_scene();
    ffe::raise_last_gl_error();

    m_scene->m_window.set_fbo_id(m_gl_scene->defaultFramebufferObject());

    m_sync_lock = m_server->sync_safe_point();

    {
        std::lock_guard<std::mutex> guard(m_sim_callback_queue_mutex);
        for (auto &cb: m_sim_callback_queue) {
            cb();
        }
        m_sim_callback_queue.clear();
    }

    if (m_curr_tool) {
        Vector3f cursor;
        bool valid = false;
        if (m_curr_tool->uses_hover()) {
            std::tie(valid, cursor) = m_curr_tool->hover(m_mouse_pos_win);
        }

        if (!valid) {
            std::tie(valid, cursor) = get_mouse_world_pos();
        }

        if (m_curr_tool->uses_brush()) {
            update_brush(cursor, valid);
        } else {
            m_scene->m_terrain_geometry.remove_overlay(
                        m_scene->m_overlay_material);
        }
    }

    const QSize size = window()->size() * window()->devicePixelRatio();
    m_scene->update_size(size);

    /*{
        Vector3f pos = m_scene->m_camera.controller().pos();
        pos[eX] = std::max(std::min(pos[eX], float(m_terrain.size())),
                           0.f);
        pos[eY] = std::max(std::min(pos[eY], float(m_terrain.size())),
                           0.f);
        const sim::Terrain::HeightField *field = nullptr;
        auto lock = m_terrain.readonly_field(field);
        m_scene->m_camera.controller().set_pos(
                    Vector3f(pos[eX], pos[eY],
                             (*field)[(unsigned int)(pos[eY])*m_terrain.size()+(unsigned int)(pos[eX])]
                    ));
    }*/

    m_scene->m_camera.sync();
    m_scene->m_scenegraph.sync();
    m_gl_scene->setup_scene(&m_scene->m_rendergraph);
    ffe::raise_last_gl_error();
}

void TerraformMode::resizeEvent(QResizeEvent *event)
{
    ApplicationMode::resizeEvent(event);
    const QSize size = m_gl_scene->size() * window()->devicePixelRatio();
    m_viewport_size = ffe::ViewportSize(size.width(), size.height());
    if (m_tool_backend) {
        m_tool_backend->set_viewport_size(m_viewport_size);
    }
}

void TerraformMode::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_scene) {
        return;
    }

    const Vector2f new_pos = Vector2f(event->pos().x(),
                                      event->pos().y());
    const Vector2f dist = m_mouse_pos_win - new_pos;
    m_mouse_pos_win = new_pos;

    switch (m_mouse_mode) {
    case MOUSE_ROTATE:
    {
        m_scene->m_camera.controller().set_rot(
                    m_scene->m_camera.controller().rot() + Vector2f(dist[eY], dist[eX])*0.002);
        break;
    }
    case MOUSE_TOOL_DRAG:
    {
        if (m_drag->is_continuous()) {
            break;
        }

        sim::WorldOperationPtr op(m_drag->drag(m_mouse_pos_win));
        if (op) {
            m_server->enqueue_op(std::move(op));
        }
        break;
    }
    default:;
    }

}

void TerraformMode::mousePressEvent(QMouseEvent *event)
{
    m_mouse_pos_win = Vector2f(event->pos().x(), event->pos().y());

    if (m_mouse_mode != MOUSE_IDLE) {
        return;
    }

    m_mouse_dispatcher.dispatch(event);
}

void TerraformMode::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_mouse_action != nullptr) {
        if (m_mouse_action->mouse_binding().is_valid() &&
                event->button() == m_mouse_action->mouse_binding().button())
        {
            clear_mouse_mode();
        }
        return;
    }
}

void TerraformMode::wheelEvent(QWheelEvent *event)
{
    if (!m_scene) {
        return;
    }

    m_scene->m_camera.controller().set_distance(
                m_scene->m_camera.controller().distance()-event->angleDelta().y()/10.
                );
}

void TerraformMode::clear_mouse_mode()
{
    releaseMouse();
    if (m_mouse_mode == MOUSE_TOOL_DRAG) {
        m_drag->done(m_mouse_pos_win);
        m_drag = nullptr;
    }
    m_mouse_mode = MOUSE_IDLE;
    m_mouse_action = nullptr;
}

void TerraformMode::enter_mouse_mode(MouseMode mode, MouseAction *original_action)
{
    grabMouse();
    m_mouse_mode = mode;
    m_mouse_action = original_action;
}

std::pair<bool, Vector3f> TerraformMode::get_mouse_world_pos()
{
    bool valid;
    Vector3f p;
    const Ray r = m_scene->m_camera.ray(m_mouse_pos_win, m_viewport_size);
    std::tie(p, valid) = m_terrain_interface->hittest(r);
    return std::make_pair(valid, p);
}

void TerraformMode::initialise_tools()
{
    m_tool_backend = std::make_unique<ToolBackend>(m_brush_frontend,
                                                   m_server->state(),
                                                   m_scene->m_scenegraph.root(),
                                                   m_scene->m_octree_group,
                                                   m_scene->m_camera,
                                                   m_sim_callback_queue_mutex,
                                                   m_sim_callback_queue);

    const QSize size = window()->size() * window()->devicePixelRatio();
    m_tool_backend->set_viewport_size(Vector2f(size.width(), size.height()));

    // terrain tools
    m_tool_raise_lower = std::make_unique<TerraRaiseLowerTool>(*m_tool_backend);
    m_tool_ramp = std::make_unique<TerraRampTool>(*m_tool_backend);
    m_tool_smooth = std::make_unique<TerraSmoothTool>(*m_tool_backend);
    m_tool_level = std::make_unique<TerraLevelTool>(*m_tool_backend);
    connect(m_tool_level.get(), &TerraLevelTool::reference_height_changed,
            this, &TerraformMode::on_tool_level_reference_height_changed);

    // fluid tools
    m_tool_fluid_raise = std::make_unique<TerraFluidRaiseTool>(*m_tool_backend);
    m_tool_fluid_source = std::make_unique<TerraFluidSourceTool>(
                *m_tool_backend,
                m_scene->m_fluid_source_material,
                m_scene->m_fancy_drag_plane_material);

    // testing tools
    m_tool_testing = std::make_unique<TerraTestingTool>(*m_tool_backend,
                                                        m_scene->m_bezier_material,
                                                        m_scene->m_road_material);

    m_ui->action_terraform_tool_terrain_raise_lower->trigger();
}

bool TerraformMode::may_clear_mouse_mode(MouseMode potential_mode)
{
    if (m_mouse_mode != MOUSE_IDLE) {
        if (m_mouse_mode == potential_mode) {
            clear_mouse_mode();
        }
        return true;
    }
    return false;
}

void TerraformMode::switch_to_tool(AbstractTerraTool *new_tool)
{
    if (new_tool == m_curr_tool) {
        return;
    }

    auto lock = m_server->sync_safe_point();

    if (m_curr_tool) {
        m_curr_tool->deactivate();
    }

    m_curr_tool = new_tool;

    bool any_settings = false;

    any_settings = any_settings || m_curr_tool->uses_brush();
    m_ui->brush_settings->setVisible(m_curr_tool->uses_brush());

    m_ui->level_tool_settings->setVisible(m_curr_tool == m_tool_level.get());

    m_ui->tool_settings_frame->setVisible(any_settings);

    if (m_curr_tool) {
        m_curr_tool->activate();
    }
}

void collect_octree_aabbs(std::vector<AABB> &dest, const ffe::OctreeNode &node)
{
    dest.emplace_back(node.bounds());
    for (unsigned int i = 0; i < 8; ++i) {
        const ffe::OctreeNode *child = node.child(i);
        if (child) {
            collect_octree_aabbs(dest, *child);
        }
    }
}

void TerraformMode::collect_aabbs(std::vector<AABB> &dest)
{
    dest.clear();
    collect_octree_aabbs(dest, m_scene->m_octree_group.octree().root());
}

void TerraformMode::load_brushes()
{
    for (unsigned int i = 0;
         i < sizeof(brush_search_paths)/sizeof(const char*);
         i++)
    {
        logger.logf(io::LOG_DEBUG, "searching for brushes in: %s",
                    brush_search_paths[i]);
        load_brushes_from(QDir(brush_search_paths[i]));
    }
}

void TerraformMode::load_brushes_from(QDir dir, bool recurse)
{
    for (QFileInfo &file: dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
    {
        if (file.isDir() && recurse) {
            load_brushes_from(QDir(file.absoluteFilePath()));
            continue;
        }

        if (!file.isFile()) {
            continue;
        }

        QString filename = file.baseName();
        gamedata::PixelBrushDef brush;
        QByteArray data;
        {
            QFile f(file.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly)) {
                logger.logf(io::LOG_WARNING,
                            "failed to open potential brush: %s",
                            filename.constData());
                continue;
            }
            data = f.readAll();
        }

        if (brush.ParseFromArray(data.constData(), data.size())) {
            logger.logf(io::LOG_DEBUG, "loaded PixelBrush from %s",
                        filename.toStdString().c_str());
            m_brush_objects.append(brush);
        } else if (load_gimp_brush((const uint8_t*)data.constData(), data.size(), brush)) {
            logger.logf(io::LOG_DEBUG, "loaded gimp brush from %s",
                        filename.toStdString().c_str());
            m_brush_objects.append(brush);
        } else {
            logger.logf(io::LOG_WARNING, "failed to load any brush from %s",
                        filename.toStdString().c_str());
        }
    }
}

void TerraformMode::prepare_scene()
{
    if (m_scene) {
        return;
    }

    const QSize size = window()->size() * window()->devicePixelRatio();

    m_scene = std::make_unique<TerraformScene>(
                *m_terrain_interface,
                m_server->state(),
                size,
                std::bind(&TerraformMode::collect_aabbs,
                          this,
                          std::placeholders::_1)
                );

    // TerraformScene &scene = *m_scene;

    /*for (const sim::Fluid::Source *source: m_server.state().fluid().sources())
    {
        ffe::FluidSource &rendernode = scene.m_octree_group.root().emplace<ffe::FluidSource>(
                    scene.m_fluid_source_material);
        rendernode.update_from_source(source);
    }*/

    initialise_tools();
}

void TerraformMode::update_brush(const Vector3f &cursor, bool cursor_valid)
{
    Brush *const curr_brush = m_brush_frontend.curr_brush();
    if (m_brush_changed) {
        if (curr_brush) {
            std::vector<Brush::density_t> buf(129*129);
            curr_brush->preview_buffer(129, buf);
            m_scene->m_brush.bind();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 129, 129, GL_RED, GL_FLOAT,
                            buf.data());
        }
        m_brush_changed = false;
    }
    if (curr_brush && cursor_valid) {
        float brush_radius = m_brush_frontend.brush_size()/2.f;

        ffe::ShaderProgram &shader = m_scene->m_overlay_material.make_pass_material(
                    m_scene->m_solid_pass).shader();
        shader.bind();
        glUniform2f(shader.uniform_location("location"),
                    cursor[eX], cursor[eY]);
        glUniform1f(shader.uniform_location("radius"),
                    brush_radius);
        m_scene->m_terrain_geometry.configure_overlay(
                    m_scene->m_overlay_material,
                    sim::TerrainRect(std::max(0.f, std::floor(cursor[eX]-brush_radius)),
                                     std::max(0.f, std::floor(cursor[eY]-brush_radius)),
                                     std::ceil(cursor[eX]+brush_radius),
                                     std::ceil(cursor[eY]+brush_radius)));
    } else {
        m_scene->m_terrain_geometry.remove_overlay(m_scene->m_overlay_material);
    }
}

void TerraformMode::activate(QWidget &parent)
{
    ApplicationMode::activate(parent);
    m_advance_conn = connect(m_gl_scene, &OpenGLScene::advance,
                             this, &TerraformMode::advance,
                             Qt::DirectConnection);
    m_before_gl_sync_conn = connect(m_gl_scene, &OpenGLScene::before_gl_sync,
                                    this, &TerraformMode::before_gl_sync,
                                    Qt::DirectConnection);
    m_after_gl_sync_conn = connect(m_gl_scene, &OpenGLScene::after_gl_sync,
                                   this, &TerraformMode::after_gl_sync,
                                   Qt::DirectConnection);

    m_server = std::make_unique<sim::Server>();
    m_terrain_interface = std::make_unique<ffe::FancyTerrainInterface>(m_server->state().terrain(), 61);
    m_server->state().terrain().notify_heightmap_changed();
}

void TerraformMode::deactivate()
{
    if (m_tools.checkedAction()) {
        m_tools.checkedAction()->toggle();
    }

    disconnect(m_after_gl_sync_conn);
    disconnect(m_advance_conn);
    disconnect(m_before_gl_sync_conn);

    m_tool_fluid_raise = nullptr;
    m_tool_fluid_source = nullptr;
    m_tool_level = nullptr;
    m_tool_raise_lower = nullptr;
    m_tool_ramp = nullptr;
    m_tool_smooth = nullptr;
    m_tool_testing = nullptr;
    m_tool_backend = nullptr;
    m_server = nullptr;
    m_terrain_interface = nullptr;
    m_scene = nullptr;
    ApplicationMode::deactivate();
}

BrushList *TerraformMode::brush_list_model()
{
    return &m_brush_objects;
}

std::tuple<Vector3f, bool> TerraformMode::hittest(const Vector2f viewport)
{
    if (!m_scene) {
        return std::make_tuple(Vector3f(), false);
    }
    const Ray ray = m_scene->m_camera.ray(viewport, m_viewport_size);

    return m_terrain_interface->hittest(ray);
}

void TerraformMode::on_action_terraform_tool_terrain_raise_lower_triggered()
{
    switch_to_tool(m_tool_raise_lower.get());
}

void TerraformMode::on_action_terraform_tool_terrain_level_triggered()
{
    switch_to_tool(m_tool_level.get());
    on_tool_level_reference_height_changed(m_tool_level->reference_height());
}

void TerraformMode::on_action_terraform_tool_terrain_smooth_triggered()
{
    switch_to_tool(m_tool_smooth.get());
}

void TerraformMode::on_action_terraform_tool_terrain_ramp_triggered()
{
    switch_to_tool(m_tool_ramp.get());
}

void TerraformMode::on_action_terraform_tool_fluid_raise_lower_triggered()
{
    switch_to_tool(m_tool_fluid_raise.get());
}

void TerraformMode::on_slider_brush_size_valueChanged(int value)
{
    if (value < 0 || value > 1024) {
        return;
    }
    m_brush_frontend.set_brush_size(value);
    m_ui->brush_size_label->setNum(value);
}

void TerraformMode::on_slider_brush_strength_valueChanged(int value)
{
    const float scaled_value = (float)value / (float)m_ui->slider_brush_strength->maximum();
    m_brush_frontend.set_brush_strength(scaled_value);
    m_ui->brush_intensity_label->setText(QString::asprintf("%.3f", scaled_value));
}

void TerraformMode::on_brush_list_clicked(const QModelIndex &index)
{
    m_brush_frontend.set_brush(
                m_brush_objects.vector()[index.row()]->m_brush.get()
            );
    m_brush_changed = true;
}

void TerraformMode::on_action_terraform_tool_test_triggered()
{
    switch_to_tool(m_tool_testing.get());
}

void TerraformMode::on_action_terraform_tool_fluid_edit_sources_triggered()
{
    switch_to_tool(m_tool_fluid_source.get());
}

void TerraformMode::on_camera_pan_triggered()
{
    if (!m_scene) {
        return;
    }

    if (may_clear_mouse_mode(MOUSE_TOOL_DRAG)) {
        return;
    }

    bool valid;
    Vector3f world_pos;
    std::tie(valid, world_pos) = get_mouse_world_pos();
    if (valid) {
        m_drag = std::make_unique<CameraDrag>(m_scene->m_camera,
                                              m_tool_backend->viewport_size(),
                                              world_pos);
        enter_mouse_mode(MOUSE_TOOL_DRAG, m_app.shared_actions().action_camera_pan);
    }
}

void TerraformMode::on_camera_rotate_triggered()
{
    if (!m_scene) {
        return;
    }

    if (may_clear_mouse_mode(MOUSE_ROTATE)) {
        return;
    }

    enter_mouse_mode(MOUSE_ROTATE, m_app.shared_actions().action_camera_rotate);
}

void TerraformMode::on_tool_primary_triggered()
{
    if (!m_scene) {
        return;
    }

    if (may_clear_mouse_mode(MOUSE_TOOL_DRAG)) {
        return;
    }

    if (!m_curr_tool) {
        return;
    }

    ToolDragPtr drag = nullptr;
    sim::WorldOperationPtr op;
    {
        auto lock = m_server->sync_safe_point();
        std::tie(drag, op) = m_curr_tool->primary_start(m_mouse_pos_win);
    }

    if (drag) {
        m_drag = std::move(drag);
        enter_mouse_mode(MOUSE_TOOL_DRAG, m_app.shared_actions().action_tool_primary);
    }
    if (op) {
        m_server->enqueue_op(std::move(op));
    }
}

void TerraformMode::on_tool_secondary_triggered()
{
    if (!m_scene) {
        return;
    }

    if (may_clear_mouse_mode(MOUSE_TOOL_DRAG)) {
        return;
    }

    if (!m_curr_tool) {
        return;
    }

    ToolDragPtr drag = nullptr;
    sim::WorldOperationPtr op = nullptr;
    {
        auto lock = m_server->sync_safe_point();
        std::tie(drag, op) = m_curr_tool->secondary_start(m_mouse_pos_win);
    }

    if (drag) {
        m_drag = std::move(drag);
        enter_mouse_mode(MOUSE_TOOL_DRAG, m_app.shared_actions().action_tool_secondary);
    }
    if (op) {
        m_server->enqueue_op(std::move(op));
    }
}

void TerraformMode::on_tool_level_reference_height_changed(float new_value)
{
    m_ui->level_tool_reference_height_slider->setValue(std::round(new_value * 100.f));
}

void TerraformMode::on_level_tool_reference_height_slider_valueChanged(int value)
{
    if (!m_curr_tool || m_curr_tool != m_tool_level.get()) {
        return;
    }

    const float scaled_value = float(value) / 100.f;
    m_tool_level->set_reference_height(scaled_value);
    m_ui->level_tool_reference_height_label->setText(QString::asprintf("%.2f", scaled_value));
}
