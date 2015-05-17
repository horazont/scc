/**********************************************************************
File name: mainmenu.hpp
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
#ifndef SCC_MAINMENU_H
#define SCC_MAINMENU_H

#include "fixups.hpp"

#include "ffengine/common/resource.hpp"
#include "ffengine/render/scenegraph.hpp"

#include "mode.hpp"

class QuickGLScene;


struct MainMenuScene
{
    engine::RenderGraph m_rendergraph;
    engine::ResourceManager m_resources;
    engine::SceneGraph m_scenegraph;
    engine::PerspectivalCamera m_camera;
};


class MainMenu: public ApplicationMode
{
    Q_OBJECT
public:
    MainMenu(QQmlEngine *engine);
    ~MainMenu() override;

private:
    std::unique_ptr<MainMenuScene> m_scene;

    QMetaObject::Connection m_gl_sync_conn;

protected:
    void prepare_scene();

signals:

public slots:
    void before_gl_sync();

public:
    void activate(Application &app, QQuickItem &parent) override;
    void deactivate() override;


};

#endif
