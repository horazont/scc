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
#include "ui_mainmenu.h"

#include "application.hpp"
#include "terraform/terraform.hpp"

MainMenu::MainMenu(Application &app, QWidget *parent) :
    ApplicationMode(app, parent),
    m_ui(new Ui::MainMenu)
{
    m_ui->setupUi(this);

    m_ui->btn_singleplayer->set_action(m_ui->action_mainmenu_singleplayer);
    m_ui->btn_multiplayer->set_action(m_ui->action_mainmenu_multiplayer);
    m_ui->btn_map_editor->set_action(m_ui->action_mainmenu_map_editor);
    m_ui->btn_settings->set_action(m_ui->action_mainmenu_settings);
    m_ui->btn_quit->set_action(m_ui->action_mainmenu_quit);

    {
        std::string group_name(QT_TRANSLATE_NOOP("Bindings", "Main menu"));
        m_app.keybindings().add_item(group_name, m_ui->action_mainmenu_map_editor);
        m_app.keybindings().add_item(group_name, m_ui->action_mainmenu_multiplayer);
        m_app.keybindings().add_item(group_name, m_ui->action_mainmenu_quit);
        m_app.keybindings().add_item(group_name, m_ui->action_mainmenu_singleplayer);
        m_app.keybindings().add_item(group_name, m_ui->action_mainmenu_settings);
    }
}

MainMenu::~MainMenu()
{
    delete m_ui;
}

void MainMenu::on_action_mainmenu_map_editor_triggered()
{
    m_app.enter_mode(Application::TERRAFORM);
}

void MainMenu::on_action_mainmenu_quit_triggered()
{
    m_app.quit();
}

void MainMenu::on_action_mainmenu_settings_triggered()
{
    m_app.show_preferences_dialog();
}
