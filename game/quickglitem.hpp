#ifndef SCC_GAME_QUICKGLITEM_H
#define SCC_GAME_QUICKGLITEM_H

#include <GL/glew.h>

#include <chrono>
#include <memory>

#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>

#include "engine/gl/vbo.hpp"
#include "engine/gl/shader.hpp"
#include "engine/gl/ibo.hpp"
#include "engine/gl/vao.hpp"
#include "engine/gl/texture.hpp"
#include "engine/gl/ubo.hpp"

#include "engine/render/scenegraph.hpp"
#include "engine/render/terrain.hpp"

#include "engine/io/filesystem.hpp"

#include "engine/sim/terrain.hpp"

typedef std::chrono::high_resolution_clock hrclock;
typedef std::chrono::steady_clock monoclock;


class QuickGLScene: public QObject
{
    Q_OBJECT
public:
    explicit QuickGLScene(QObject *parent = 0);
    ~QuickGLScene();

private:
    bool m_initialized;
    engine::ResourceManager m_resources;
    engine::PerspectivalCamera m_camera;
    sim::Terrain m_terrain;
    engine::SceneGraph m_scenegraph;
    hrclock::time_point m_t;
    monoclock::time_point m_t0;
    unsigned int m_nframes;
    engine::scenegraph::Transformation *m_pointer_parent;
    std::unique_ptr<engine::scenegraph::Node> m_tmp_pointer;

public:
    engine::Texture2D &test_texture;

public:
    inline sim::Terrain &terrain()
    {
        return m_terrain;
    }

    inline engine::PerspectivalCamera &camera()
    {
        return m_camera;
    }

public slots:
    void paint();

public:
    void advance(engine::TimeInterval seconds);
    void boost_camera(const Vector2f &by);
    void boost_camera_rot(const Vector2f &by);
    void set_viewport_size(const QSize &size);
    void set_pointer_position(const Vector3f &pos);
    void set_pointer_visible(bool visible);
    void sync();
    void zoom_camera(const float by);

};

class QuickGLItem: public QQuickItem
{
    Q_OBJECT

public:
    QuickGLItem(QQuickItem *parent = 0);

private:
    io::FileSystem m_vfs;
    std::unique_ptr<QuickGLScene> m_renderer;
    Vector2f m_hover_pos;
    monoclock::time_point m_t;
    bool m_dragging;
    Vector3f m_drag_camera_pos;
    Vector3f m_drag_point;

protected:
    void hoverMoveEvent(QHoverEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    virtual QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *);

public:
    std::tuple<Vector3f, bool> hittest(const Vector2f viewport);

public slots:
    void cleanup();
    void new_frame();
    void sync();

private slots:
    void handle_window_changed(QQuickWindow *win);

};

#endif // SCC_GAME_QUICKGLITEM_H
