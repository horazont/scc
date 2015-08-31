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


TerraformScene::TerraformScene():
    m_sphere_material(engine::VBOFormat({engine::VBOAttribute(3)}))
{
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
    append(std::unique_ptr<Brush>(new ImageBrush(brush)),
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
    m_curr_tool(&m_tool_raise_lower),
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

    m_brush_frontend.set_brush(&m_test_brush);
    m_brush_frontend.set_brush_size(32);

    m_brush_objects.append(std::unique_ptr<Brush>(new ParzenBrush()), "Parzen");
    m_brush_objects.append(std::unique_ptr<Brush>(new CircleBrush()), "Circle");

    load_brushes();

    m_brush_frontend.set_brush(m_brush_objects.vector()[0]->m_brush.get());
    m_brush_frontend.set_brush_size(32);
    m_curr_tool = &m_tool_raise_lower;

    m_brush_frontend.set_brush_strength(10.0);
    apply_tool(15, 20, false);
    m_brush_frontend.set_brush_strength(1.0);

    m_curr_tool = &m_tool_level;
    m_tool_level.set_value(0.f);
    m_brush_frontend.set_brush_strength(5.0);
    m_brush_frontend.set_brush_size(64);
    apply_tool(30, 20, false);

    m_server.enqueue_op(std::make_unique<sim::ops::FluidSourceCreate>(10, 20, 5, 1, 0.3));
    m_server.enqueue_op(std::make_unique<sim::ops::FluidSourceCreate>(80, 20, 5, 8, 0.3));

    m_curr_tool = &m_tool_smooth;
    m_brush_frontend.set_brush_strength(1.0);
    m_brush_frontend.set_brush_size(4);
    apply_tool(100, 100, false);

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
    }
}

void TerraformMode::advance(engine::TimeInterval dt)
{
    if (m_scene) {
        m_scene->m_camera.advance(dt);
        m_scene->m_scenegraph.advance(dt);
        m_scene->m_water_scenegraph.advance(dt);
        if (!m_paused) {
            m_t += dt;
            m_scene->m_sphere_rot->set_rotation(Quaternionf::rot(m_t * M_PI / 10., Vector3f(1, 1, 1).normalized()));
        }
    }
    if (m_mouse_mode == MOUSE_PAINT) {
        ensure_mouse_world_pos();
        if (m_mouse_world_pos_valid) {
            apply_tool(m_mouse_world_pos[eX], m_mouse_world_pos[eY],
                       m_paint_secondary);
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
        m_ui->ldebug_octree_selected_objects->setNum((int)m_scene->m_octree_group->selected_objects());
        m_ui->ldebug_fps->setNum(std::round(m_gl_scene->fps()));
    }
}

void TerraformMode::before_gl_sync()
{
    engine::raise_last_gl_error();
    prepare_scene();
    engine::raise_last_gl_error();
    m_scene->m_camera.sync();
    engine::raise_last_gl_error();

    m_scene->m_window.set_fbo_id(m_gl_scene->defaultFramebufferObject());

    Brush *const curr_brush = m_brush_frontend.curr_brush();
    ensure_mouse_world_pos();
    if (m_brush_changed) {
        if (curr_brush) {
            std::vector<Brush::density_t> buf(129*129);
            curr_brush->preview_buffer(129, buf);
            m_scene->m_brush->bind();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 129, 129, GL_RED, GL_FLOAT,
                            buf.data());
        }
        m_brush_changed = false;
    }
    if (curr_brush && m_mouse_world_pos_valid) {
        float brush_radius = m_brush_frontend.brush_size()/2.f;

        m_scene->m_overlay->shader().bind();
        glUniform2f(m_scene->m_overlay->shader().uniform_location("location"),
                    m_mouse_world_pos[eX], m_mouse_world_pos[eY]);
        glUniform1f(m_scene->m_overlay->shader().uniform_location("radius"),
                    brush_radius);
        m_scene->m_terrain_node->configure_overlay(
                    *m_scene->m_overlay,
                    sim::TerrainRect(std::max(0.f, std::floor(m_mouse_world_pos[eX]-brush_radius)),
                                     std::max(0.f, std::floor(m_mouse_world_pos[eY]-brush_radius)),
                                     std::ceil(m_mouse_world_pos[eX]+brush_radius),
                                     std::ceil(m_mouse_world_pos[eY]+brush_radius)));
    } else {
        m_scene->m_terrain_node->remove_overlay(*m_scene->m_overlay);
    }
    if (m_mouse_world_pos_valid) {
        m_scene->m_pointer_trafo_node->transformation() = translation4(m_mouse_world_pos);
    }

    const QSize size = window()->size() * window()->devicePixelRatio();
    if (m_scene->m_window.width() != size.width() ||
            m_scene->m_window.height() != size.height() )
    {
        // resize window
        m_scene->m_window.set_size(size.width(), size.height());
        // resize FBO
        /*(*m_scene->m_prewater_pass) = engine::FBO(size.width(), size.height());
        m_scene->m_prewater_colour_buffer->bind();
        m_scene->m_prewater_colour_buffer->reinit(GL_RGBA, size.width(), size.height());
        m_scene->m_prewater_depth_buffer->bind();
        m_scene->m_prewater_depth_buffer->reinit(GL_DEPTH_COMPONENT24, size.width(), size.height());
        m_scene->m_prewater_pass->attach(GL_COLOR_ATTACHMENT0, m_scene->m_prewater_colour_buffer);
        m_scene->m_prewater_pass->attach(GL_DEPTH_ATTACHMENT, m_scene->m_prewater_depth_buffer);*/
    }

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

    Vector3f pos = m_scene->m_camera.controller().pos();
    m_scene->m_fluidplane_trafo_node->transformation() = translation4(
                Vector3f(std::round(pos[eX])+0.5, std::round(pos[eY])+0.5, 0.f));

    m_gl_scene->setup_scene(&m_scene->m_rendergraph);
    engine::raise_last_gl_error();
}

void TerraformMode::resizeEvent(QResizeEvent *event)
{
    ApplicationMode::resizeEvent(event);
    const QSize size = m_gl_scene->size() * window()->devicePixelRatio();
    m_viewport_size = engine::ViewportSize(size.width(), size.height());
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
        m_scene->m_pointer_trafo_node->transformation() = translation4(
                    now_at);
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
                m_curr_tool->secondary_start(m_mouse_world_pos[eX],
                                             m_mouse_world_pos[eY]);
            } else {
                m_curr_tool->primary_start(m_mouse_world_pos[eX],
                                           m_mouse_world_pos[eY]);
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

void TerraformMode::apply_tool(const float x0,
                               const float y0,
                               bool secondary)
{
    if (m_curr_tool) {
        auto lock = m_server.sync_safe_point();
        sim::WorldOperationPtr op(nullptr);
        if (secondary) {
            op = m_curr_tool->secondary(x0, y0);
        } else {
            op = m_curr_tool->primary(x0, y0);
        }
        if (op) {
            m_server.enqueue_op(std::move(op));
        }
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
    collect_octree_aabbs(dest, m_scene->m_octree_group->octree().root());
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

    m_scene = std::unique_ptr<TerraformScene>(new TerraformScene());
    TerraformScene &scene = *m_scene;

    const QSize size = window()->size() * window()->devicePixelRatio();
    scene.m_window.set_size(size.width(), size.height());

    /*scene.m_prewater_pass = &scene.m_resources.emplace<engine::FBO>(
                "fbo/prewater",
                size.width(), size.height());
    engine::raise_last_gl_error();
    scene.m_prewater_colour_buffer = &scene.m_resources.emplace<engine::Texture2D>(
                "fbo/prewater/colour0",
                GL_RGBA,
                size.width(), size.height());
    scene.m_prewater_colour_buffer->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    scene.m_prewater_depth_buffer = &scene.m_resources.emplace<engine::Texture2D>(
                "fbo/prewater/depth",
                GL_DEPTH_COMPONENT24,
                size.width(), size.height(),
                GL_DEPTH_COMPONENT, GL_FLOAT);
    scene.m_prewater_depth_buffer->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    engine::raise_last_gl_error();
    scene.m_prewater_pass->attach(GL_DEPTH_ATTACHMENT, scene.m_prewater_depth_buffer);
    scene.m_prewater_pass->attach(GL_COLOR_ATTACHMENT0, scene.m_prewater_colour_buffer);*/

    engine::SceneRenderNode &scene_node = scene.m_rendergraph.new_node<engine::SceneRenderNode>(
                scene.m_window,
                scene.m_scenegraph,
                scene.m_camera);
    scene_node.set_clear_mask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    scene_node.set_clear_colour(Vector4f(0.5, 0.4, 0.3, 1.0));

    /*engine::BlitNode &blit_node = scene.m_rendergraph.new_node<engine::BlitNode>(
                *scene.m_prewater_pass,
                scene.m_window);
    blit_node.dependencies().push_back(&scene_node);

    engine::SceneRenderNode &water_node = scene.m_rendergraph.new_node<engine::SceneRenderNode>(
                scene.m_window,
                scene.m_water_scenegraph,
                scene.m_camera);
    water_node.set_clear_mask(0);
    water_node.dependencies().push_back(&blit_node);*/

    if (!scene.m_rendergraph.resort()) {
        throw std::runtime_error("rendergraph has cycles");
    }

    /*scene.m_scenegraph.root().emplace<engine::GridNode>(1024, 1024, 8);*/

    scene.m_camera.controller().set_distance(40.0);
    scene.m_camera.controller().set_rot(Vector2f(-60.f/180.f*M_PI, 0));
    scene.m_camera.controller().set_pos(Vector3f(20, 30, 20));
    scene.m_camera.set_fovy(60. / 180. * M_PI);
    scene.m_camera.set_zfar(10000.0);
    scene.m_camera.set_znear(1.0);

    scene.m_grass = &scene.m_resources.emplace<engine::Texture2D>(
                "grass", GL_RGBA, 512, 512);
    scene.m_grass->bind();
    load_image_to_texture(":/textures/grass00.png");

    scene.m_rock = &scene.m_resources.emplace<engine::Texture2D>(
                "rock", GL_RGBA, 512, 512);
    scene.m_rock->bind();
    load_image_to_texture(":/textures/rock00.png");

    scene.m_blend = &scene.m_resources.emplace<engine::Texture2D>(
                "blend", GL_RED, 256, 256);
    scene.m_blend->bind();
    load_image_to_texture(":/textures/blend00.png");

    scene.m_waves = &scene.m_resources.emplace<engine::Texture2D>(
                "waves", GL_RGBA, 512, 512);
    scene.m_waves->bind();
    load_image_to_texture(":/textures/waves00.png");
    engine::raise_last_gl_error();

    engine::FullTerrainNode &full_terrain = scene.m_scenegraph.root().emplace<engine::FullTerrainNode>(
                m_terrain_interface.size(),
                m_terrain_interface.grid_size());

    scene.m_terrain_node = &full_terrain.emplace<engine::FancyTerrainNode>(
                m_terrain_interface,
                scene.m_resources);
    scene.m_terrain_node->attach_grass_texture(scene.m_grass);
    scene.m_terrain_node->attach_rock_texture(scene.m_rock);
    scene.m_terrain_node->attach_blend_texture(scene.m_blend);
    engine::raise_last_gl_error();

    scene.m_pointer_trafo_node = &scene.m_scenegraph.root().emplace<
            engine::scenegraph::Transformation>();
    scene.m_pointer_trafo_node->emplace_child<engine::PointerNode>(1.0);
    engine::raise_last_gl_error();

    scene.m_overlay = &scene.m_resources.manage<engine::Material>(
                "materials/overlay",
                std::move(engine::Material::shared_with(
                              scene.m_terrain_node->material()))
                );
    spp::EvaluationContext ctx(scene.m_resources.shader_library());
    scene.m_overlay->shader().attach(
                scene.m_resources.load_shader_checked(":/shaders/terrain/brush_overlay.frag"),
                ctx,
                GL_FRAGMENT_SHADER);
    if (!scene.m_terrain_node->configure_overlay_material(*scene.m_overlay)) {
        throw std::runtime_error("shader failed to compile or link");
    }
    scene.m_terrain_node->configure_overlay(
                *scene.m_overlay,
                sim::TerrainRect(70, 70, 250, 250));
    engine::raise_last_gl_error();

    scene.m_brush = &scene.m_resources.emplace<engine::Texture2D>(
                "textures/brush", GL_R32F, 129, 129, GL_RED, GL_FLOAT);
    engine::raise_last_gl_error();
    scene.m_brush->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    engine::raise_last_gl_error();

    scene.m_overlay->shader().bind();
    glUniform2f(scene.m_overlay->shader().uniform_location("center"),
                160, 160);
    scene.m_overlay->attach_texture("brush", scene.m_brush);
    engine::raise_last_gl_error();

    scene.m_fluidplane_trafo_node = &scene.m_water_scenegraph.root().emplace<
            engine::scenegraph::Transformation>();
    engine::raise_last_gl_error();

    /*engine::FluidNode &plane_node = scene.m_fluidplane_trafo_node->emplace_child<engine::FluidNode>(
                m_server.state().fluid());
    engine::raise_last_gl_error();
    plane_node.attach_waves_texture(scene.m_waves);
    engine::raise_last_gl_error();
    plane_node.attach_scene_colour_texture(scene.m_prewater_colour_buffer);
    engine::raise_last_gl_error();
    plane_node.attach_scene_depth_texture(scene.m_prewater_depth_buffer);
    engine::raise_last_gl_error();*/


    bool success = scene.m_sphere_material.shader().attach_resource(
                GL_VERTEX_SHADER,
                ":/shaders/oct_sphere/main.vert");
    success = success && scene.m_sphere_material.shader().attach_resource(
                GL_FRAGMENT_SHADER,
                ":/shaders/oct_sphere/main.frag");

    scene.m_sphere_material.declare_attribute("position", 0);

    success = success && scene.m_sphere_material.link();

    if (!success) {
        throw std::runtime_error("failed to compile or link shader");
    }

    scene.m_scenegraph.root().emplace<engine::DynamicAABBs>(
                std::bind(&TerraformMode::collect_aabbs,
                          this,
                          std::placeholders::_1));

    scene.m_octree_group = &scene.m_scenegraph.root().emplace<engine::scenegraph::OctreeGroup>();
    scene.m_sphere_rot = &scene.m_octree_group->root().emplace<engine::scenegraph::OctRotation>();

    /*engine::scenegraph::OctGroup &sphere_group = scene.m_sphere_rot->emplace_child<engine::scenegraph::OctGroup>();

    for (double t = 0; t <= 2; t += 0.02) {
        engine::scenegraph::OctRotation &rot = sphere_group.emplace<engine::scenegraph::OctRotation>();
        rot.set_rotation(Quaternionf::rot(4*t*M_PI, Vector3f(0, 0, 1)));
        engine::scenegraph::OctTranslation &tx = rot.emplace_child<engine::scenegraph::OctTranslation>();
        tx.set_translation(Vector3f(t*40, 0, 30+15*t));
        tx.emplace_child<engine::OctSphere>(scene.m_sphere_material, 1.5);
    }*/

    scene.m_sphere_material.sync();

    scene.m_bezier_material = &scene.m_resources.emplace<engine::Material>(
                "material/sphere",
                engine::VBOFormat({engine::VBOAttribute(4)}));

    success = scene.m_bezier_material->shader().attach_resource(
                GL_VERTEX_SHADER,
                ":/shaders/curve/main.vert");
    success = success && scene.m_bezier_material->shader().attach_resource(
                GL_FRAGMENT_SHADER,
                ":/shaders/curve/main.frag");

    scene.m_bezier_material->declare_attribute("position_t", 0);

    success = success && scene.m_bezier_material->link();

    if (!success) {
        throw std::runtime_error("shader failed to link or compile");
    }

    m_tool_backend.set_sgnode(&scene.m_octree_group->root());
    m_tool_testing.set_preview_material(*scene.m_bezier_material);
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
    m_curr_tool = &m_tool_raise_lower;
}

void TerraformMode::on_tool_terrain_flatten_triggered()
{
    m_curr_tool = &m_tool_level;
}

void TerraformMode::on_tool_terrain_smooth_triggered()
{
    m_curr_tool = &m_tool_smooth;
}

void TerraformMode::on_tool_terrain_ramp_triggered()
{
    m_curr_tool = &m_tool_ramp;
}

void TerraformMode::on_tool_fluid_raise_lower_triggered()
{
    m_curr_tool = &m_tool_fluid_raise;
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
    m_curr_tool = &m_tool_testing;
}
