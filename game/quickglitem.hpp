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
    engine::SceneGraph m_scenegraph;
    hrclock::time_point m_t;
    monoclock::time_point m_t0;
    unsigned int m_nframes;
    Vector2f m_pos;

public slots:
    void paint();

public:
    void set_pos(const QPoint &pos);
    void set_viewport_size(const QSize &size);
    void sync();

};

class QuickGLItem: public QQuickItem
{
    Q_OBJECT

public:
    QuickGLItem(QQuickItem *parent = 0);

private:
    std::unique_ptr<QuickGLScene> m_renderer;
    QPoint m_hover_pos;

protected:
    void hoverMoveEvent(QHoverEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    virtual QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *);

public slots:
    void cleanup();
    void sync();

private slots:
    void handle_window_changed(QQuickWindow *win);

};

#endif // SCC_GAME_QUICKGLITEM_H
