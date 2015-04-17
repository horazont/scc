#include "terraform.hpp"

#include <QQmlContext>
#include <QQuickWindow>

#include "engine/io/filesystem.hpp"

#include "engine/math/intersect.hpp"
#include "engine/math/perlin.hpp"
#include "engine/math/algo.hpp"

#include "engine/render/grid.hpp"
#include "engine/render/pointer.hpp"

#include "application.hpp"

static io::Logger &logger = io::logging().get_logger("app.terraform");


void load_image_to_texture(const QString &url)
{
    QImage texture = QImage(url);
    texture.convertToFormat(QImage::Format_ARGB32);

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


TerraformMode::TerraformMode(QQmlEngine *engine):
    ApplicationMode("Terraform", engine, QUrl("qrc:/qml/Terra.qml")),
    m_terrain(4097),
    m_terrain_interface(m_terrain, 65),
    m_dragging(false),
    m_tool(TerraformTool::RAISE),
    m_brush_changed(true)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    /* m_terrain.from_perlin(PerlinNoiseGenerator(Vector3(2048, 2048, 0),
                                               Vector3(13, 13, 3),
                                               0.45,
                                               12,
                                               128));*/
    /* m_terrain.from_sincos(Vector3f(0.4, 0.4, 1.2)); */
    m_terrain.notify_heightmap_changed();

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
}

void TerraformMode::advance(engine::TimeInterval dt)
{
    if (m_scene) {
        m_scene->m_camera.advance(dt);
        m_scene->m_scenegraph.advance(dt);
    }
    if (dt > 0.02) {
        logger.logf(io::LOG_WARNING, "long frame: %.4f seconds", dt);
    }
}

void TerraformMode::before_gl_sync()
{
    prepare_scene();
    m_scene->m_camera.sync();

    Brush *const curr_brush = m_brush_frontend.curr_brush();
    if (m_brush_changed) {
        if (curr_brush) {
            std::vector<Brush::density_t> buf(65*65, 100);
            const float scale = 1./32;
            for (int y = -32; y <= 32; y++) {
                for (int x = -32; x <= 32; x++) {
                    buf[(y+32)*65 + (x+32)] = curr_brush->sample(x*scale, y*scale);
                }
            }
            m_scene->m_brush->bind();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 65, 65, GL_RED, GL_FLOAT,
                            buf.data());
        } else {
            m_scene->m_terrain_node->remove_overlay(*m_scene->m_overlay);
        }
        m_brush_changed = false;
    }
    if (curr_brush) {
        float brush_radius = m_brush_frontend.brush_size()/2.f;

        m_scene->m_overlay->shader().bind();
        glUniform2f(m_scene->m_overlay->shader().uniform_location("location"),
                    m_hover_world[eX], m_hover_world[eY]);
        glUniform1f(m_scene->m_overlay->shader().uniform_location("radius"),
                    brush_radius);
        m_scene->m_terrain_node->configure_overlay(
                    *m_scene->m_overlay,
                    sim::TerrainRect(std::max(0.f, std::floor(m_hover_world[eX]-brush_radius)),
                                     std::max(0.f, std::floor(m_hover_world[eY]-brush_radius)),
                                     std::ceil(m_hover_world[eX]+brush_radius),
                                     std::ceil(m_hover_world[eY]+brush_radius)));
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
    m_gl_scene->setup_scene(&m_scene->m_rendergraph);
    const QSize size = window()->size() * window()->devicePixelRatio();
}

void TerraformMode::geometryChanged(const QRectF &oldSize,
                                    const QRectF &newSize)
{
    QQuickItem::geometryChanged(oldSize, newSize);
    std::cout << "viewport changed" << std::endl;
    const QSize size = window()->size() * window()->devicePixelRatio();
    m_viewport_size = engine::ViewportSize(size.width(), size.height());
    if (m_scene) {
        m_scene->m_window.set_size(size.width(), size.height());
    }
}

void TerraformMode::hoverMoveEvent(QHoverEvent *event)
{
    if (!m_scene) {
        return;
    }

    const Vector2f new_pos = Vector2f(event->pos().x(),
                                      event->pos().y());
    m_hover_pos = new_pos;
    Vector3f hit_pos;
    bool hit;
    std::tie(hit_pos, hit) = hittest(new_pos);
    if (hit) {
        m_hover_world = hit_pos;
        m_scene->m_pointer_trafo_node->transformation() = translation4(
                    hit_pos);
    }
}

void TerraformMode::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_scene) {
        return;
    }

    const Vector2f new_pos = Vector2f(event->pos().x(),
                                      event->pos().y());
    const Vector2f dist = m_hover_pos - new_pos;

    m_hover_pos = new_pos;

    if (event->buttons() & Qt::RightButton) {
        /* m_scene->m_camera.controller().boost_rotation(Vector2f(dist[eY], dist[eX])); */
        m_scene->m_camera.controller().set_rot(m_scene->m_camera.controller().rot() + Vector2f(dist[eY], dist[eX])*0.002);
    }

    if (m_dragging) {
        const Ray viewray = m_scene->m_camera.ray(m_hover_pos, m_viewport_size);
        PlaneSide side;
        float t;
        std::tie(t, side) = isect_plane_ray(
                    Plane(Vector3f(0, 0, m_drag_point[eZ]),
                          Vector3f(0, 0, 1)),
                    viewray);
        if (side == PlaneSide::NEGATIVE_NORMAL) {
            /* qml_gl_logger.log(io::LOG_WARNING,
                              "drag followup hittest failed, "
                              "disabling dragging"); */
            m_dragging = false;
        } else {
            const Vector3f now_at = viewray.origin + viewray.direction*t;
            const Vector3f moved = m_drag_point - now_at;
            m_scene->m_camera.controller().set_pos(m_drag_camera_pos + moved);
            m_drag_camera_pos = m_scene->m_camera.controller().pos();
            m_hover_world = now_at;
            m_scene->m_pointer_trafo_node->transformation() = translation4(
                        now_at);
        }
    }
}

void TerraformMode::mousePressEvent(QMouseEvent *event)
{
    m_hover_pos = Vector2f(event->pos().x(), event->pos().y());
    if (event->button() == Qt::MiddleButton) {
        std::tie(m_drag_point, m_dragging) = hittest(m_hover_pos);
        if (m_dragging) {
            m_drag_camera_pos = m_scene->m_camera.controller().pos();
        }
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
    }
}

void TerraformMode::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_dragging = false;
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

void TerraformMode::prepare_scene()
{
    if (m_scene) {
        return;
    }

    m_scene = std::unique_ptr<TerraformScene>(new TerraformScene());
    TerraformScene &scene = *m_scene;

    const QSize size = window()->size() * window()->devicePixelRatio();
    scene.m_window.set_size(size.width(), size.height());

    engine::SceneRenderNode &scene_node = scene.m_rendergraph.new_node<engine::SceneRenderNode>(
                scene.m_window,
                scene.m_rendergraph.new_scene(
                    scene.m_scenegraph,
                    scene.m_camera));
    scene_node.set_clear_mask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    scene_node.set_clear_colour(Vector4f(0.5, 0.4, 0.3, 1.0));

    scene.m_rendergraph.resort();

    /* scene.m_scenegraph.root().emplace<engine::GridNode>(2048, 2048, 64); */

    scene.m_camera.controller().set_distance(50.0);
    scene.m_camera.controller().set_rot(Vector2f(-45, 0));
    scene.m_camera.controller().set_pos(Vector3f(0, 0, 0.));
    /* scene.m_camera.set_fovy(45.); */
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

    scene.m_terrain_node = &scene.m_scenegraph.root().emplace<engine::FancyTerrainNode>(
                m_terrain_interface, 32);
    scene.m_terrain_node->attach_grass_texture(scene.m_grass);
    scene.m_terrain_node->attach_rock_texture(scene.m_rock);

    scene.m_pointer_trafo_node = &scene.m_scenegraph.root().emplace<
            engine::scenegraph::Transformation>();
    scene.m_pointer_trafo_node->emplace_child<engine::PointerNode>(1.0);

    scene.m_overlay = &scene.m_resources.emplace<engine::Material>("materials/overlay");
    scene.m_overlay->shader().attach_resource(GL_FRAGMENT_SHADER,
                                              ":/shaders/terrain/brush_overlay.frag");
    scene.m_terrain_node->configure_overlay_material(*scene.m_overlay);
    scene.m_terrain_node->configure_overlay(*scene.m_overlay, sim::TerrainRect(70, 70, 250, 250));

    scene.m_brush = &scene.m_resources.emplace<engine::Texture2D>(
                "textures/brush", GL_R32F, 65, 65, GL_RED, GL_FLOAT);
    engine::raise_last_gl_error();
    scene.m_brush->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    scene.m_overlay->shader().bind();
    glUniform2f(scene.m_overlay->shader().uniform_location("center"),
                160, 160);
    scene.m_overlay->attach_texture("brush", scene.m_brush);

}

void TerraformMode::apply_tool(const unsigned int x0,
                               const unsigned int y0)
{
    const float brush_radius = m_brush_frontend.brush_size()/2.f;
    sim::TerrainRect r(x0, y0, std::ceil(x0+brush_radius), std::ceil(y0+brush_radius));
    if (x0 < std::ceil(brush_radius)) {
        r.set_x0(0);
    } else {
        r.set_x0(x0-std::ceil(brush_radius));
    }
    if (y0 < std::ceil(brush_radius)) {
        r.set_y0(0);
    } else {
        r.set_y0(y0-std::ceil(brush_radius));
    }
    if (r.x1() > m_terrain.size()) {
        r.set_x1(m_terrain.size());
    }
    if (r.y1() > m_terrain.size()) {
        r.set_y1(m_terrain.size());
    }

    {
        sim::Terrain::HeightField *heightmap = nullptr;
        auto lock = m_terrain.writable_field(heightmap);

        switch (m_tool) {
        case TerraformTool::FLATTEN:
        {
            tool_flatten(*heightmap, r, x0, y0);
            break;
        }
        case TerraformTool::RAISE:
        {
            tool_raise(*heightmap, x0-brush_radius, y0-brush_radius);
            break;
        }
        case TerraformTool::LOWER:
        {
            tool_lower(*heightmap, r);
            break;
        }
        }
    }
    m_terrain.notify_heightmap_changed(r);
}

void TerraformMode::tool_flatten(sim::Terrain::HeightField &field,
                                 const sim::TerrainRect &r,
                                 const unsigned int x0,
                                 const unsigned int y0)
{
    const sim::Terrain::height_t height = field[y0*m_terrain.size()+x0];
    for (unsigned int y = r.y0(); y < r.y1(); y++) {
        for (unsigned int x = r.x0(); x < r.x1(); x++) {
            field[y*m_terrain.size()+x] = height;
        }
    }
}

void TerraformMode::tool_raise(sim::Terrain::HeightField &field,
                               const float x0,
                               const float y0)
{
    const int terrain_xbase= std::round(x0);
    const int terrain_ybase = std::round(y0);
    const unsigned int size = m_brush_frontend.brush_size();

    const std::vector<Brush::density_t> &sampled = m_brush_frontend.sampled();

    for (int y = 0; y < size; y++) {
        const int yterrain = y + terrain_ybase;
        if (yterrain < 0) {
            continue;
        }
        if (yterrain >= (int)m_terrain.size()) {
            break;
        }
        for (int x = 0; x < size; x++) {
            const int xterrain = x + terrain_xbase;
            if (xterrain < 0) {
                continue;
            }
            if (xterrain >= (int)m_terrain.size()) {
                break;
            }

            sim::Terrain::height_t &h = field[yterrain*m_terrain.size()+xterrain];
            h = std::max(sim::Terrain::min_height,
                         std::min(sim::Terrain::max_height,
                                  h + sampled[y*size+x]));
        }
    }
}

void TerraformMode::tool_lower(sim::Terrain::HeightField &field,
                               const sim::TerrainRect &r)
{
    for (unsigned int y = r.y0(); y < r.y1(); y++) {
        for (unsigned int x = r.x0(); x < r.x1(); x++) {
            field[y*m_terrain.size()+x] -= 1;
        }
    }
}

void TerraformMode::activate(Application &app, QQuickItem &parent)
{
    ApplicationMode::activate(app, parent);
    m_advance_conn = connect(m_gl_scene, &QuickGLScene::advance,
                             this, &TerraformMode::advance,
                             Qt::QueuedConnection);
    m_before_gl_sync_conn = connect(m_gl_scene, &QuickGLScene::before_gl_sync,
                                    this, &TerraformMode::before_gl_sync,
                                    Qt::DirectConnection);
}

void TerraformMode::deactivate()
{
    disconnect(m_before_gl_sync_conn);
    ApplicationMode::deactivate();
}

std::tuple<Vector3f, bool> TerraformMode::hittest(const Vector2f viewport)
{
    if (!m_scene) {
        return std::make_tuple(Vector3f(), false);
    }
    const Ray ray = m_scene->m_camera.ray(viewport, m_viewport_size);

    return m_terrain_interface.hittest(ray);
}

void TerraformMode::switch_to_tool_flatten()
{
    m_tool = TerraformTool::FLATTEN;
}

void TerraformMode::switch_to_tool_lower()
{
    m_tool = TerraformTool::LOWER;
}

void TerraformMode::switch_to_tool_raise()
{
    m_tool = TerraformTool::RAISE;
}

void TerraformMode::set_brush_strength(float strength)
{
    m_brush_frontend.set_brush_strength(strength);
}

void TerraformMode::set_brush_size(float size)
{
    if (size < 0 || size > 1024) {
        return;
    }
    m_brush_frontend.set_brush_size(std::round(size));
}
