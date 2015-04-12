#include "quickglscene.hpp"

#include <QSurface>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QQuickWindow>

#include <QSGOpacityNode>

#include "engine/io/log.hpp"

static io::Logger &qml_gl_logger = io::logging().get_logger("app.qgl");


QuickGLScene::QuickGLScene():
    m_t(monoclock::now()),
    m_rendergraph(nullptr),
    m_render_rendergraph(nullptr)
{
    setFlags(QQuickItem::ItemHasContents);
    connect(this, SIGNAL(windowChanged(QQuickWindow*)),
            this, SLOT(window_changed(QQuickWindow*)));
}

QuickGLScene::~QuickGLScene()
{

}

QSGNode *QuickGLScene::updatePaintNode(QSGNode *oldNode,
                                       UpdatePaintNodeData*)
{
    update();
    if (!oldNode) {
        oldNode = new QSGOpacityNode();
    }
    oldNode->markDirty(QSGNode::DirtyForceUpdate);
    return oldNode;
}

void QuickGLScene::before_rendering()
{
    const monoclock::time_point now = monoclock::now();
    const engine::TimeInterval dt = std::chrono::duration_cast<
            std::chrono::duration<float, std::ratio<1> >
            >(now - m_t).count();

    /* qml_gl_logger.logf(io::LOG_DEBUG, "frametime: %.4f s", dt); */

    emit advance(dt);

    m_t = now;
}

void QuickGLScene::cleanup()
{
    throw std::runtime_error("cleanup not implemented");
}

void QuickGLScene::paint()
{
    /* glClearColor(0.5, 0.4, 0.3, 1.0); */
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_render_rendergraph) {
        glGetError();
        m_render_rendergraph->render();
        engine::raise_last_gl_error();
    } else {
        qml_gl_logger.log(io::LOG_WARNING, "nothing to draw");
    }
}

void QuickGLScene::sync()
{
    emit before_gl_sync();
    m_render_rendergraph = m_rendergraph;
    if (m_render_rendergraph) {
        m_render_rendergraph->sync();
    }
    emit after_gl_sync();
}

void QuickGLScene::window_changed(QQuickWindow *win)
{
    if (!win) {
        qml_gl_logger.log(io::LOG_WARNING, "lost window!");
        return;
    }

    qml_gl_logger.log(io::LOG_INFO, "initializing window...");

    connect(win, SIGNAL(beforeSynchronizing()),
            this, SLOT(sync()),
            Qt::DirectConnection);
    connect(win, SIGNAL(sceneGraphInvalidated()),
            this, SLOT(cleanup()),
            Qt::DirectConnection);
    connect(win, SIGNAL(beforeRendering()),
            this, SLOT(before_rendering()),
            Qt::QueuedConnection);
    connect(win, SIGNAL(beforeRendering()),
            this, SLOT(paint()),
            Qt::DirectConnection);

    win->setSurfaceType(QSurface::OpenGLSurface);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(0);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(0);
    format.setStencilBufferSize(8);
    format.setDepthBufferSize(24);

    win->setFormat(format);
    win->create();

    QOpenGLContext *context = new QOpenGLContext();
    context->setFormat(format);
    if (!context->create()) {
        throw std::runtime_error("failed to create context");
    }

    qml_gl_logger.log(io::LOG_INFO)
            << "created context, version "
            << context->format().majorVersion()
            << "."
            << context->format().minorVersion()
            << io::submit;

    bool context_ok = true;
    io::LogLevel context_info_level = io::LOG_DEBUG;
    if (context->format().profile() != QSurfaceFormat::CoreProfile ||
            context->format().majorVersion() != 3 ||
            context->format().depthBufferSize() == 0)
    {
        context_ok = false;
        context_info_level = io::LOG_WARNING;
        qml_gl_logger.log(io::LOG_EXCEPTION)
                << "Could not create Core-profile OpenGL 3+ context with depth buffer"
                << io::submit;
    } else {
        qml_gl_logger.log(io::LOG_DEBUG,
                          "context deemed appropriate, continuing...");
    }

    qml_gl_logger.log(context_info_level)
            << "  renderable  : "
            << (context->format().renderableType() == QSurfaceFormat::OpenGL
                ? "OpenGL"
                : context->format().renderableType() == QSurfaceFormat::OpenGLES
                ? "OpenGL ES"
                : context->format().renderableType() == QSurfaceFormat::OpenVG
                ? "OpenVG (software?)"
                : "unknown")
            << io::submit;
    qml_gl_logger.log(context_info_level)
            << "  rgba        : "
            << context->format().redBufferSize() << " "
            << context->format().greenBufferSize() << " "
            << context->format().blueBufferSize() << " "
            << context->format().alphaBufferSize() << " "
            << io::submit;
    qml_gl_logger.log(context_info_level)
            << "  stencil     : "
            << context->format().stencilBufferSize()
            << io::submit;
    qml_gl_logger.log(context_info_level)
            << "  depth       : " << context->format().depthBufferSize()
            << io::submit;
    qml_gl_logger.log(context_info_level)
            << "  multisamples: " << context->format().samples()
            << io::submit;
    qml_gl_logger.log(context_info_level)
            << "  profile     : "
            << (context->format().profile() == QSurfaceFormat::CoreProfile
                ? "core"
                : "compatibility")
            << io::submit;

    if (!context_ok) {
        throw std::runtime_error("Failed to create appropriate OpenGL context");
    }

    context->makeCurrent(win);

    qml_gl_logger.log(io::LOG_INFO)
            << "initializing GLEW in experimental mode"
            << io::submit;
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        const std::string error = std::string((const char*)glewGetErrorString(err));
        qml_gl_logger.log(io::LOG_EXCEPTION)
                << "GLEW failed to initialize"
                << error
                << io::submit;
        throw std::runtime_error("failed to initialize GLEW: " + error);
    }

    qml_gl_logger.log(io::LOG_DEBUG) << "turning off clear" << io::submit;
    win->setClearBeforeRendering(false);

    qml_gl_logger.log(io::LOG_INFO) << "Window and rendering context initialized :)"
                                    << io::submit;
}

void QuickGLScene::setup_scene(engine::RenderGraph *rendergraph)
{
    m_rendergraph = rendergraph;
}
