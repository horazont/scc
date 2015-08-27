/**********************************************************************
File name: openglscene.cpp
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
#include "fixups.hpp"

#include "openglscene.hpp"
#include "ui_openglscene.h"

#include <QWindow>

static io::Logger &logger = io::logging().get_logger("app.glscene");

OpenGLScene::OpenGLScene(QWidget *parent) :
    QOpenGLWidget(parent),
    m_ui(new Ui::OpenGLScene),
    m_t(monoclock::now()),
    m_rendergraph(nullptr),
    m_previous_t(m_t),
    m_frames(0)
{
    m_ui->setupUi(this);
}

OpenGLScene::~OpenGLScene()
{
    delete m_ui;
}

void OpenGLScene::advance_frame()
{
    monoclock::time_point t_now = monoclock::now();
    auto seconds_passed =
            std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1> > >(t_now-m_t).count();
    emit advance(seconds_passed);
    m_t = t_now;
}

void OpenGLScene::initializeGL()
{
    QSurfaceFormat format = this->format();

    logger.log(io::LOG_INFO)
            << "created context, version "
            << format.majorVersion()
            << "."
            << format.minorVersion()
            << io::submit;

    io::LogLevel context_info_level = io::LOG_DEBUG;
    if (format.profile() != QSurfaceFormat::CoreProfile ||
            format.majorVersion() != 3 ||
            format.depthBufferSize() == 0)
    {
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
            << (format.renderableType() == QSurfaceFormat::OpenGL
                ? "OpenGL"
                : format.renderableType() == QSurfaceFormat::OpenGLES
                ? "OpenGL ES"
                : format.renderableType() == QSurfaceFormat::OpenVG
                ? "OpenVG (software?)"
                : "unknown")
            << io::submit;
    logger.log(context_info_level)
            << "  rgba        : "
            << format.redBufferSize() << " "
            << format.greenBufferSize() << " "
            << format.blueBufferSize() << " "
            << format.alphaBufferSize() << " "
            << io::submit;
    logger.log(context_info_level)
            << "  stencil     : "
            << format.stencilBufferSize()
            << io::submit;
    logger.log(context_info_level)
            << "  depth       : " << format.depthBufferSize()
            << io::submit;
    logger.log(context_info_level)
            << "  multisamples: " << format.samples()
            << io::submit;
    logger.log(context_info_level)
            << "  profile     : "
            << (format.profile() == QSurfaceFormat::CoreProfile
                ? "core"
                : "compatibility")
            << io::submit;

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
}

void OpenGLScene::resizeGL(int, int)
{

}

void OpenGLScene::paintGL()
{
    glGetError();
    emit before_gl_sync();
    if (m_rendergraph) {
        m_rendergraph->sync();
    }
    emit after_gl_sync();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

#ifdef TIMELOG_QUICKGLSCENE
    timelog_clock::time_point t0 = timelog_clock::now();
    timelog_clock::time_point t_render;
#endif

    if (m_rendergraph) {
        glGetError();
        m_rendergraph->render();
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
                std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1> > >(t_now-m_previous_t).count();
        if (seconds_passed > 1.f) {
            m_previous_fps = m_frames / seconds_passed;
            logger.logf(io::LOG_DEBUG, "%.0f FPS", m_previous_fps);
            m_previous_t = t_now;
            m_frames = 0;
        }
    }

    if (m_rendergraph) {
        update();
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    advance_frame();
}

double OpenGLScene::fps()
{
    return m_previous_fps;
}

void OpenGLScene::setup_scene(engine::RenderGraph *rendergraph)
{
    m_rendergraph = rendergraph;
    update();
}

