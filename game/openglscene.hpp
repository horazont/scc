/**********************************************************************
File name: openglscene.hpp
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
#ifndef OPENGLSCENE_HPP
#define OPENGLSCENE_HPP

#include "fixups.hpp"

#include <chrono>

#include <QOpenGLWidget>

#include "ffengine/render/scenegraph.hpp"
#include "ffengine/render/camera.hpp"
#include "ffengine/render/renderpass.hpp"

typedef std::chrono::steady_clock monoclock;

namespace Ui {
class OpenGLScene;
}

class OpenGLScene : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit OpenGLScene(QWidget *parent = 0);
    ~OpenGLScene();

private:
    Ui::OpenGLScene *m_ui;

    monoclock::time_point m_t;

    ffe::RenderGraph *m_rendergraph;

    monoclock::time_point m_previous_t;
    double m_previous_fps;
    unsigned int m_frames;

signals:
    void advance(ffe::TimeInterval seconds);
    void after_gl_sync();
    void before_gl_sync();

protected:
    void advance_frame();
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

public:
    double fps();
    void setup_scene(ffe::RenderGraph *rendergraph);

};

#endif // OPENGLSCENE_H
