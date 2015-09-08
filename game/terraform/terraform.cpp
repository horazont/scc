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
    m_brush(m_resources.emplace<ffe::Texture2D>(
                "runtime/brush_texture",
                GL_R32F, 129, 129,
                GL_RED,
                GL_FLOAT)),
    m_octree_group(m_scenegraph.root().emplace<ffe::scenegraph::OctreeGroup>())
{
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


TerraformMode::TerraformMode(QWidget *parent):
    ApplicationMode(parent),
    m_ui(new Ui::TerraformMode),
    m_server(),
    m_terrain_interface(m_server.state().terrain(), 61),
    m_t(100),
    m_mouse_mode(MOUSE_IDLE),
    m_paint_secondary(false),
    m_mouse_world_pos_updated(false),
    m_mouse_world_pos_valid(false),
    m_brush_changed(true),
    m_tool_backend(m_brush_frontend, m_server.state()),
    m_tool_raise_lower(m_tool_backend),
    m_tool_level(m_tool_backend),
    m_tool_smooth(m_tool_backend),
    m_tool_ramp(m_tool_backend),
    m_tool_fluid_raise(m_tool_backend),
    m_tool_testing(m_tool_backend),
    m_curr_tool(nullptr),
    m_brush_objects(this),
    m_paused(false)
{
    m_ui->setupUi(this);
    setMouseTracking(true);
    m_ui->toolbtn_terrain_raise_lower->setDefaultAction(m_ui->tool_terrain_raise_lower);
    m_ui->toolbtn_terrain_flatten->setDefaultAction(m_ui->tool_terrain_flatten);
    m_ui->toolbtn_terrain_smooth->setDefaultAction(m_ui->tool_terrain_smooth);
    m_ui->toolbtn_terrain_ramp->setDefaultAction(m_ui->tool_terrain_ramp);
    m_ui->toolbtn_fluid_raise_lower->setDefaultAction(m_ui->tool_fluid_raise_lower);
    m_ui->toolbtn_testing->setDefaultAction(m_ui->tool_testing);

    m_ui->tabWidget->tabBar()->setDrawBase(false);

    m_ui->brush_list->setModel(&m_brush_objects);

    setFocusPolicy(Qt::StrongFocus);

    /* m_terrain.from_perlin(PerlinNoiseGenerator(Vector3(2048, 2048, 0),
                                               Vector3(13, 13, 3),
                                               0.45,
                                               12,
                                               128));*/
    /* m_terrain.from_sincos(Vector3f(0.4, 0.4, 1.2)); */
    m_server.state().terrain().notify_heightmap_changed();

    /* // simple gaussian brush
    const float sigma = 9.0;
    for (unsigned int y = 0; y < 65; y++)
    {
        for (unsigned int x = 0; x < 65; x++)
        {
            const float r = std::sqrt(sqr(float(x) - 32)+sqr(float(y) - 32));
            float density = std::exp(-sqr(r)/(2*sqr(sigma)));
            m_test_brush.set(x, y, density);

        }
    }*/

    switch_to_tool(&m_tool_raise_lower);

    m_brush_frontend.set_brush(&m_test_brush);
    m_brush_frontend.set_brush_size(32);

    m_brush_objects.append(std::make_unique<ParzenBrush>(), "Parzen");
    m_brush_objects.append(std::make_unique<CircleBrush>(), "Circle");

    load_brushes();

    m_brush_frontend.set_brush(m_brush_objects.vector()[0]->m_brush.get());
    m_brush_frontend.set_brush_size(32);

    switch_to_tool(&m_tool_raise_lower);

    m_brush_frontend.set_brush_strength(10.0);
    apply_tool(Vector2f(), Vector3f(15, 20, 0), false);
    m_brush_frontend.set_brush_strength(1.0);

    switch_to_tool(&m_tool_level);

    m_tool_level.set_value(0.f);
    m_brush_frontend.set_brush_strength(5.0);
    m_brush_frontend.set_brush_size(64);
    apply_tool(Vector2f(), Vector3f(30, 20, 0), false);

    m_server.enqueue_op(std::make_unique<sim::ops::FluidSourceCreate>(10, 20, 5, 1, 0.3));
    m_server.enqueue_op(std::make_unique<sim::ops::FluidSourceCreate>(80, 20, 5, 8, 0.3));

    m_curr_tool = &m_tool_smooth;
    m_brush_frontend.set_brush_strength(1.0);
    m_brush_frontend.set_brush_size(4);
    apply_tool(Vector2f(), Vector3f(100, 100, 0), false);

    m_ui->slider_brush_size->setValue(64);
    m_ui->slider_brush_strength->setValue(m_ui->slider_brush_strength->maximum());
}

TerraformMode::~TerraformMode()
{
    delete m_ui;
}

void TerraformMode::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
    {
        m_paused = !m_paused;
        break;
    }
    case Qt::Key_R:
    {
        if (m_scene) {
            logger.logf(io::LOG_INFO, "re-balancing octree on user request");
            timelog_clock::time_point t0 = timelog_clock::now();
            timelog_clock::time_point t1;
            m_scene->m_octree_group.octree().rebalance();
            t1 = timelog_clock::now();
            logger.logf(io::LOG_INFO, "rebalance() took %.2f ms", ms_cast(t1-t0).count());
        }
    }
    }
}

void TerraformMode::advance(ffe::TimeInterval dt)
{
    if (m_scene) {
        m_scene->m_camera.advance(dt);
        m_scene->m_scenegraph.advance(dt);
    }
    if (m_mouse_mode == MOUSE_PAINT) {
        ensure_mouse_world_pos();
        if (m_mouse_world_pos_valid) {
            apply_tool(m_mouse_pos_win, m_mouse_world_pos, m_paint_secondary);
        }
        // terrain under mouse probably changed
        m_mouse_world_pos_updated = false;
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

    if (m_curr_tool && m_curr_tool->uses_brushes()) {
        update_brush();
    }

    if (m_curr_tool && m_curr_tool->uses_hover()) {
        ensure_mouse_world_pos();

        Vector3f cursor;
        bool valid = m_mouse_world_pos_valid;
        if (valid) {
            std::tie(valid, cursor) = m_curr_tool->hover(m_mouse_pos_win,
                                                         m_mouse_world_pos);
        }
        if (valid) {
            // FIXME: m_scene->m_pointer_trafo_node->transformation() = translation4(cursor);
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

    m_sync_lock = m_server.sync_safe_point();

    m_gl_scene->setup_scene(&m_scene->m_rendergraph);
    ffe::raise_last_gl_error();
}

void TerraformMode::resizeEvent(QResizeEvent *event)
{
    ApplicationMode::resizeEvent(event);
    const QSize size = m_gl_scene->size() * window()->devicePixelRatio();
    m_viewport_size = ffe::ViewportSize(size.width(), size.height());
    m_tool_backend.set_viewport_size(m_viewport_size);
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
    m_mouse_world_pos_updated = false;

    switch (m_mouse_mode) {
    case MOUSE_ROTATE:
    {
        m_scene->m_camera.controller().set_rot(
                    m_scene->m_camera.controller().rot() + Vector2f(dist[eY], dist[eX])*0.002);
        break;
    }
    case MOUSE_DRAG:
    {
        const Ray viewray = m_scene->m_camera.ray(m_mouse_pos_win,
                                                  m_viewport_size);
        PlaneSide side;
        float t;
        std::tie(t, side) = isect_plane_ray(
                    Plane(m_drag_point[eZ], Vector3f(0, 0, 1)),
                    viewray);
        if (side != PlaneSide::BOTH) {
            m_mouse_mode = MOUSE_IDLE;
            break;
        }

        const Vector3f now_at = viewray.origin + viewray.direction*t;
        const Vector3f moved = m_drag_point - now_at;
        m_scene->m_camera.controller().set_pos(m_drag_camera_pos + moved);
        m_drag_camera_pos = m_scene->m_camera.controller().pos();
        m_mouse_world_pos = now_at;
        m_mouse_world_pos_updated = true;
        // FIXME: m_scene->m_pointer_trafo_node->transformation() = translation4(now_at);
        break;
    }
    default:;
    }

}

void TerraformMode::mousePressEvent(QMouseEvent *event)
{
    if (m_mouse_mode != MOUSE_IDLE) {
        return;
    }

    m_mouse_pos_win = Vector2f(event->pos().x(), event->pos().y());
    m_mouse_world_pos_updated = false;

    switch (event->button())
    {
    case Qt::RightButton:
    case Qt::LeftButton:
    {
        m_paint_secondary = (event->button() == Qt::RightButton);
        m_mouse_mode = MOUSE_PAINT;
        ensure_mouse_world_pos();
        if (m_mouse_world_pos_valid) {
            if (m_paint_secondary) {
                m_curr_tool->secondary_start(m_mouse_pos_win,
                                             m_mouse_world_pos);
            } else {
                m_curr_tool->primary_start(m_mouse_pos_win,
                                           m_mouse_world_pos);
            }
        }
        break;
    }
    case Qt::MiddleButton:
    {
        if (event->modifiers() & Qt::ShiftModifier) {
            ensure_mouse_world_pos();
            if (m_mouse_world_pos_valid) {
                m_drag_point = m_mouse_world_pos;
                m_drag_camera_pos = m_scene->m_camera.controller().pos();
                m_mouse_mode = MOUSE_DRAG;
            }
        } else {
            m_mouse_mode = MOUSE_ROTATE;
        }
        break;
    }
    default:;
    }

    /* if (event->button() == Qt::MiddleButton) {
    } else if (event->button() == Qt::LeftButton) {
        Vector3f pos;
        bool hit;
        std::tie(pos, hit) = hittest(m_hover_pos);
        if (hit) {
            std::cout << pos << std::endl;
            const int x = round(pos[eX]);
            const int y = round(pos[eY]);
            std::cout << x << ", " << y << std::endl;
            if (x >= 0 && x < m_terrain.size() &&
                    y >= 0 && y < m_terrain.size())
            {
                apply_tool(x, y);
            }
        }
    }*/
}

void TerraformMode::mouseReleaseEvent(QMouseEvent *event)
{
    switch (m_mouse_mode)
    {
    case MOUSE_PAINT:
    {
        if (event->button() & (Qt::RightButton | Qt::LeftButton)) {
            m_mouse_mode = MOUSE_IDLE;
        }
        break;
    }
    case MOUSE_DRAG:
    case MOUSE_ROTATE:
    {
        if (event->button() == Qt::MiddleButton) {
            m_mouse_mode = MOUSE_IDLE;
        }
        break;
    }
    default:;
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

void TerraformMode::apply_tool(const Vector2f &viewport_pos,
                               const Vector3f &world_pos,
                               bool secondary)
{
    if (m_curr_tool) {
        auto lock = m_server.sync_safe_point();
        sim::WorldOperationPtr op(nullptr);
        if (secondary) {
            op = m_curr_tool->secondary(viewport_pos, world_pos);
        } else {
            op = m_curr_tool->primary(viewport_pos, world_pos);
        }
        if (op) {
            m_server.enqueue_op(std::move(op));
        }
    }
}

void TerraformMode::switch_to_tool(TerraTool *new_tool)
{
    if (new_tool == m_curr_tool) {
        return;
    }

    m_curr_tool = new_tool;

    bool any_settings = false;

    any_settings = any_settings || m_curr_tool->uses_brushes();
    m_ui->brush_settings->setVisible(m_curr_tool->uses_brushes());

    m_ui->tool_settings_frame->setVisible(any_settings);
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

void TerraformMode::ensure_mouse_world_pos()
{
    if (m_mouse_world_pos_updated) {
        return;
    }
    std::tie(m_mouse_world_pos, m_mouse_world_pos_valid) = hittest(m_mouse_pos_win);
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
                m_terrain_interface,
                m_server.state(),
                size,
                std::bind(&TerraformMode::collect_aabbs,
                          this,
                          std::placeholders::_1)
                );

    TerraformScene &scene = *m_scene;

    m_tool_backend.set_sgnode(scene.m_octree_group);
    m_tool_backend.set_camera(scene.m_camera);

    m_tool_testing.set_preview_material(scene.m_bezier_material);
    m_tool_testing.set_road_material(scene.m_road_material);
}

void TerraformMode::update_brush()
{
    Brush *const curr_brush = m_brush_frontend.curr_brush();
    ensure_mouse_world_pos();
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
    if (curr_brush && m_mouse_world_pos_valid) {
        float brush_radius = m_brush_frontend.brush_size()/2.f;

        ffe::ShaderProgram &shader = m_scene->m_overlay_material.make_pass_material(
                    m_scene->m_solid_pass).shader();
        shader.bind();
        glUniform2f(shader.uniform_location("location"),
                    m_mouse_world_pos[eX], m_mouse_world_pos[eY]);
        glUniform1f(shader.uniform_location("radius"),
                    brush_radius);
        m_scene->m_terrain_geometry.configure_overlay(
                    m_scene->m_overlay_material,
                    sim::TerrainRect(std::max(0.f, std::floor(m_mouse_world_pos[eX]-brush_radius)),
                                     std::max(0.f, std::floor(m_mouse_world_pos[eY]-brush_radius)),
                                     std::ceil(m_mouse_world_pos[eX]+brush_radius),
                                     std::ceil(m_mouse_world_pos[eY]+brush_radius)));
    } else {
        m_scene->m_terrain_geometry.remove_overlay(m_scene->m_overlay_material);
    }
}

void TerraformMode::activate(Application &app, QWidget &parent)
{
    ApplicationMode::activate(app, parent);
    m_advance_conn = connect(m_gl_scene, &OpenGLScene::advance,
                             this, &TerraformMode::advance,
                             Qt::DirectConnection);
    m_before_gl_sync_conn = connect(m_gl_scene, &OpenGLScene::before_gl_sync,
                                    this, &TerraformMode::before_gl_sync,
                                    Qt::DirectConnection);
    m_after_gl_sync_conn = connect(m_gl_scene, &OpenGLScene::after_gl_sync,
                                   this, &TerraformMode::after_gl_sync,
                                   Qt::DirectConnection);
}

void TerraformMode::deactivate()
{
    disconnect(m_after_gl_sync_conn);
    disconnect(m_advance_conn);
    disconnect(m_before_gl_sync_conn);
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

    return m_terrain_interface.hittest(ray);
}

void TerraformMode::on_tool_terrain_raise_lower_triggered()
{
    switch_to_tool(&m_tool_raise_lower);
}

void TerraformMode::on_tool_terrain_flatten_triggered()
{
    switch_to_tool(&m_tool_level);
}

void TerraformMode::on_tool_terrain_smooth_triggered()
{
    switch_to_tool(&m_tool_smooth);
}

void TerraformMode::on_tool_terrain_ramp_triggered()
{
    switch_to_tool(&m_tool_ramp);
}

void TerraformMode::on_tool_fluid_raise_lower_triggered()
{
    switch_to_tool(&m_tool_fluid_raise);
}

void TerraformMode::on_slider_brush_size_valueChanged(int value)
{
    if (value < 0 || value > 1024) {
        return;
    }
    m_brush_frontend.set_brush_size(value);
}

void TerraformMode::on_slider_brush_strength_valueChanged(int value)
{
    m_brush_frontend.set_brush_strength(
                (float)value / (float)m_ui->slider_brush_strength->maximum()
                );
}

void TerraformMode::on_brush_list_clicked(const QModelIndex &index)
{
    m_brush_frontend.set_brush(
                m_brush_objects.vector()[index.row()]->m_brush.get()
            );
    m_brush_changed = true;
}

void TerraformMode::on_tool_testing_triggered()
{
    switch_to_tool(&m_tool_testing);
}
