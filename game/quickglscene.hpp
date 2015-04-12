#ifndef QUICKGLSCENE_HPP
#define QUICKGLSCENE_HPP

#include "fixups.hpp"

#include <chrono>

#include <QQuickItem>

#include "engine/render/scenegraph.hpp"
#include "engine/render/camera.hpp"

typedef std::chrono::steady_clock monoclock;


class QuickGLScene: public QQuickItem
{
    Q_OBJECT
public:
    QuickGLScene();
    ~QuickGLScene();

private:
    monoclock::time_point m_t;

    engine::RenderGraph *m_rendergraph;
    engine::RenderGraph *m_render_rendergraph;

signals:
    void advance(engine::TimeInterval seconds);
    void after_gl_sync();
    void before_gl_sync();

protected:
    QSGNode *updatePaintNode(
            QSGNode *oldNode,
            UpdatePaintNodeData *data);

public slots:
    void before_rendering();
    void cleanup();
    void paint();
    void sync();

private slots:
    void window_changed(QQuickWindow *win);

public:
    void setup_scene(engine::RenderGraph *rendergraph);

};

#endif // QUICKGLSCENE_HPP
