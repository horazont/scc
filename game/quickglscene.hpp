/**********************************************************************
File name: quickglscene.hpp
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
#ifndef QUICKGLSCENE_HPP
#define QUICKGLSCENE_HPP

#include "fixups.hpp"

#include <chrono>

#include <QQuickItem>

#include "ffengine/render/scenegraph.hpp"
#include "ffengine/render/camera.hpp"

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
