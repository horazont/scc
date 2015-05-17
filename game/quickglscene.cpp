/**********************************************************************
File name: quickglscene.cpp
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
#include "quickglscene.hpp"

#include <QSurface>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QQuickWindow>

#include <QSGOpacityNode>

#include "ffengine/io/log.hpp"

// #define TIMELOG_QUICKGLSCENE

#ifdef TIMELOG_QUICKGLSCENE

typedef std::chrono::steady_clock timelog_clock;
#define TIMELOG_ms(x) std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1000> > >(x).count()

#endif

static io::Logger &logger = io::logging().get_logger("app.qgl");


QuickGLScene::QuickGLScene():
    m_t(monoclock::now()),
    m_rendergraph(nullptr),
    m_render_rendergraph(nullptr),
    m_previous_fps(m_t),
    m_frames(0)
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
    /* throw std::runtime_error("cleanup not implemented"); */
    logger.log(io::LOG_ERROR, "cleanup not implemented");
}

void QuickGLScene::paint()
{
    /* glClearColor(0.5, 0.4, 0.3, 1.0); */
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef TIMELOG_QUICKGLSCENE
    timelog_clock::time_point t0 = timelog_clock::now();
    timelog_clock::time_point t_render;
#endif

    if (m_render_rendergraph) {
        glGetError();
        m_render_rendergraph->render();
        engine::raise_last_gl_error();
    } else {
        logger.log(io::LOG_WARNING, "nothing to draw");
    }

#ifdef TIMELOG_QUICKGLSCENE
    t_render = timelog_clock::now();
    logger.logf(io::LOG_DEBUG, "quickglscene: paint %.2f ms",
                TIMELOG_ms(t_render - t0));
#endif
    m_frames += 1;
    {
        monoclock::time_point t_now = monoclock::now();
        auto seconds_passed =
                std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1> > >(t_now-m_previous_fps).count();
        if (seconds_passed > 1.f) {
            logger.logf(io::LOG_DEBUG, "%.0f FPS", m_frames / seconds_passed);
            m_previous_fps = t_now;
            m_frames = 0;
        }
    }
}

void QuickGLScene::sync()
{
#ifdef TIMELOG_QUICKGLSCENE
    timelog_clock::time_point t0 = timelog_clock::now();
    timelog_clock::time_point t_sync;
#endif

    emit before_gl_sync();
    m_render_rendergraph = m_rendergraph;
    if (m_render_rendergraph) {
        m_render_rendergraph->sync();
    }
    emit after_gl_sync();

#ifdef TIMELOG_QUICKGLSCENE
    t_sync = timelog_clock::now();
    logger.logf(io::LOG_DEBUG, "quickglscene: sync %.2f ms",
                TIMELOG_ms(t_sync - t0));
#endif
}

void QuickGLScene::window_changed(QQuickWindow *win)
{
    if (!win) {
        logger.log(io::LOG_WARNING, "lost window!");
        return;
    }

    logger.log(io::LOG_INFO, "initializing window...");

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

    logger.log(io::LOG_INFO)
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
        logger.log(io::LOG_EXCEPTION)
                << "Could not create Core-profile OpenGL 3+ context with depth buffer"
                << io::submit;
    } else {
        logger.log(io::LOG_DEBUG,
                          "context deemed appropriate, continuing...");
    }

    logger.log(context_info_level)
            << "  renderable  : "
            << (context->format().renderableType() == QSurfaceFormat::OpenGL
                ? "OpenGL"
                : context->format().renderableType() == QSurfaceFormat::OpenGLES
                ? "OpenGL ES"
                : context->format().renderableType() == QSurfaceFormat::OpenVG
                ? "OpenVG (software?)"
                : "unknown")
            << io::submit;
    logger.log(context_info_level)
            << "  rgba        : "
            << context->format().redBufferSize() << " "
            << context->format().greenBufferSize() << " "
            << context->format().blueBufferSize() << " "
            << context->format().alphaBufferSize() << " "
            << io::submit;
    logger.log(context_info_level)
            << "  stencil     : "
            << context->format().stencilBufferSize()
            << io::submit;
    logger.log(context_info_level)
            << "  depth       : " << context->format().depthBufferSize()
            << io::submit;
    logger.log(context_info_level)
            << "  multisamples: " << context->format().samples()
            << io::submit;
    logger.log(context_info_level)
            << "  profile     : "
            << (context->format().profile() == QSurfaceFormat::CoreProfile
                ? "core"
                : "compatibility")
            << io::submit;

    if (!context_ok) {
        throw std::runtime_error("Failed to create appropriate OpenGL context");
    }

    context->makeCurrent(win);

    logger.log(io::LOG_INFO)
            << "initializing GLEW in experimental mode"
            << io::submit;
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        const std::string error = std::string((const char*)glewGetErrorString(err));
        logger.log(io::LOG_EXCEPTION)
                << "GLEW failed to initialize"
                << error
                << io::submit;
        throw std::runtime_error("failed to initialize GLEW: " + error);
    }

    logger.log(io::LOG_DEBUG) << "turning off clear" << io::submit;
    win->setClearBeforeRendering(false);

    logger.log(io::LOG_INFO) << "Window and rendering context initialized :)"
                                    << io::submit;
}

void QuickGLScene::setup_scene(engine::RenderGraph *rendergraph)
{
    m_rendergraph = rendergraph;
}
