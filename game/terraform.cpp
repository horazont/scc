#include "terraform.hpp"

#include <QQmlContext>
#include <QQuickWindow>

#include "engine/io/filesystem.hpp"

#include "engine/math/intersect.hpp"
#include "engine/math/perlin.hpp"

#include "engine/render/grid.hpp"
#include "engine/render/pointer.hpp"

#include "application.hpp"


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
    m_terrain(2049),
    m_terrain_interface(m_terrain, 65),
    m_dragging(false),
    m_tool(TerraformTool::RAISE)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    /* m_terrain.from_perlin(PerlinNoiseGenerator(Vector3(2048, 2048, 0),
                                               Vector3(13, 13, 3),
                                               0.45,
                                               12,
                                               128));*/
    m_terrain.from_sincos(Vector3f(0.4, 0.4, 1.2));
}

void TerraformMode::before_gl_sync()
{
    prepare_scene();
    m_gl_scene->setup_scene(m_scene->m_scenegraph,
                            m_scene->m_camera);
}

void TerraformMode::geometryChanged(const QRectF &oldSize,
                                    const QRectF &newSize)
{
    QQuickItem::geometryChanged(oldSize, newSize);
    std::cout << "viewport changed" << std::endl;
    if (m_scene) {
        const QSize size = window()->size() * window()->devicePixelRatio();
        m_scene->m_camera.set_viewport(size.width(), size.height());
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
        m_scene->m_pointer_trafo_node->transformation() = translation4(hit_pos);
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
        m_scene->m_camera.controller().boost_rotation(Vector2f(dist[eY], dist[eX]));
    }

    if (m_dragging) {
        const Ray viewray = m_scene->m_camera.ray(m_hover_pos);
        bool hit;
        float t;
        std::tie(t, hit) = isect_plane_ray(
                    Vector3f(0, 0, m_drag_point[eZ]),
                    Vector3f(0, 0, 1),
                    viewray);
        if (!hit) {
            /* qml_gl_logger.log(io::LOG_WARNING,
                              "drag followup hittest failed, "
                              "disabling dragging"); */
            m_dragging = false;
        } else {
            const Vector3f now_at = viewray.origin + viewray.direction*t;
            const Vector3f moved = m_drag_point - now_at;
            m_scene->m_camera.controller().set_pos(m_drag_camera_pos + moved);
            m_drag_camera_pos = m_scene->m_camera.controller().pos();
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
                m_terrain.notify_heightmap_changed();
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

    scene.m_scenegraph.root().emplace<engine::GridNode>(2048, 2048, 64);

    scene.m_camera.controller().set_distance(50.0);
    scene.m_camera.controller().set_rot(Vector2f(-45, 0));
    scene.m_camera.controller().set_pos(Vector3f(0, 0, 0.));
    /* scene.m_camera.set_fovy(45.); */
    scene.m_camera.set_zfar(10000.0);
    scene.m_camera.set_znear(1.0);

    const QSize viewport = window()->size() * window()->devicePixelRatio();
    scene.m_camera.set_viewport(viewport.width(), viewport.height());

    scene.m_grass = &scene.m_resources.emplace<engine::Texture2D>(
                "grass", GL_RGBA, 512, 512);
    scene.m_grass->bind();
    load_image_to_texture(":/textures/grass00.png");

    scene.m_terrain_node = &scene.m_scenegraph.root().emplace<engine::FancyTerrainNode>(
                m_terrain_interface, 32);
    scene.m_terrain_node->attach_grass_texture(scene.m_grass);

    scene.m_pointer_trafo_node = &scene.m_scenegraph.root().emplace<
            engine::scenegraph::Transformation>();
    scene.m_pointer_trafo_node->emplace_child<engine::PointerNode>(1.0);
}

void TerraformMode::apply_tool(const unsigned int x0,
                               const unsigned int y0)
{
    sim::TerrainRect r(x0, y0, x0+6, y0+6);
    if (x0 < 5) {
        r.set_x0(0);
    } else {
        r.set_x0(x0-5);
    }
    if (y0 < 5) {
        r.set_y0(0);
    } else {
        r.set_y0(y0-5);
    }
    if (r.x1() > m_terrain.size()) {
        r.set_x1(m_terrain.size());
    }
    if (r.y1() > m_terrain.size()) {
        r.set_y1(m_terrain.size());
    }

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
        tool_raise(*heightmap, r);
        break;
    }
    case TerraformTool::LOWER:
    {
        tool_lower(*heightmap, r);
        break;
    }
    }
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
                               const sim::TerrainRect &r)
{
    for (unsigned int y = r.y0(); y < r.y1(); y++) {
        for (unsigned int x = r.x0(); x < r.x1(); x++) {
            field[y*m_terrain.size()+x] += 1;
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
    const Ray ray = m_scene->m_camera.ray(viewport);

    float t;
    bool hit;
    std::tie(t, hit) = isect_plane_ray(Vector3f(0, 0, 0), Vector3f(0, 0, 1), ray);
    if (!hit) {
        return std::make_tuple(Vector3f(), false);
    }

    return std::make_tuple(ray.origin + ray.direction*t, true);
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
