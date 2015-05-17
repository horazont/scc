/**********************************************************************
File name: mainmenu.cpp
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
#include "mainmenu.hpp"

#include "ffengine/io/log.hpp"
static io::Logger &app_logger = io::logging().get_logger("app");


#include "application.hpp"
#include "quickglscene.hpp"


MainMenu::MainMenu(QQmlEngine *engine):
    ApplicationMode("MainMenu", engine, QUrl("qrc:/qml/MainMenu.qml"))
{

}

MainMenu::~MainMenu()
{

}

void MainMenu::prepare_scene()
{
    if (m_scene) {
        return;
    }

    m_scene = std::unique_ptr<MainMenuScene>(new MainMenuScene());
}

void MainMenu::before_gl_sync()
{
    prepare_scene();
    m_gl_scene->setup_scene(&m_scene->m_rendergraph);
}

void MainMenu::activate(Application &app, QQuickItem &parent)
{
    ApplicationMode::activate(app, parent);
    m_gl_sync_conn = connect(
                m_gl_scene, &QuickGLScene::before_gl_sync,
                this, &MainMenu::before_gl_sync,
                Qt::DirectConnection);
}

void MainMenu::deactivate()
{
    disconnect(m_gl_sync_conn);
    ApplicationMode::deactivate();
}
