#ifndef SCC_GAME_QUICKGLITEM_H
#define SCC_GAME_QUICKGLITEM_H

#include "GL/glew.h"

#include <memory>

#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>

#include "../engine/gl/vbo.h"
#include "../engine/gl/shader.h"
#include "../engine/gl/ibo.h"
#include "../engine/gl/vao.h"

class QuickGLScene: public QObject
{
    Q_OBJECT
public:
    explicit QuickGLScene(QObject *parent = 0);
    ~QuickGLScene();

private:
    bool m_initialized;
    ShaderProgram m_test_shader;
    VBO m_test_vbo;
    IBO m_test_ibo;
    VBOAllocation m_test_allocation;
    std::unique_ptr<VAO> m_test_vao;

public slots:
    void paint();

};

class QuickGLItem: public QQuickItem
{
    Q_OBJECT

public:
    QuickGLItem(QQuickItem *parent = 0);

private:
    std::unique_ptr<QuickGLScene> m_renderer;

protected:
    virtual QSGNode *updatePaintNode(QSGNode *oldNode,
                                     UpdatePaintNodeData *updatePaintNodeData);

public slots:
    void cleanup();
    void sync();

private slots:
    void handle_window_changed(QQuickWindow *win);

};

#endif // SCC_GAME_QUICKGLITEM_H
