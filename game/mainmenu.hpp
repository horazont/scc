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
#ifndef MAINMENU_HPP
#define MAINMENU_HPP

#include "mode.hpp"

namespace Ui {
class MainMenu;
}

class MainMenu: public ApplicationMode
{
    Q_OBJECT

public:
    explicit MainMenu(Application &app, QWidget *parent = 0);
    ~MainMenu();

private slots:
    void on_action_mainmenu_map_editor_triggered();

    void on_action_mainmenu_quit_triggered();

    void on_action_mainmenu_settings_triggered();

private:
    Ui::MainMenu *m_ui;

};

#endif // MAINMENU_HPP
