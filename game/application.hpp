/**********************************************************************
File name: application.hpp
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
#ifndef APPLICATION_H
#define APPLICATION_H

#include "fixups.hpp"

#include <memory>

#include <QActionGroup>
#include <QMainWindow>
#include <QMetaObject>
#include <QMdiSubWindow>

#include "ffengine/io/filesystem.hpp"

#include "mode.hpp"
#include "openglscene.hpp"
#include "bindings.hpp"


namespace Ui {
class Application;
}

class Application : public QMainWindow
{
    Q_OBJECT
public:
    enum Mode {
        MAIN_MENU,
        TERRAFORM,
    };

public:
    explicit Application(QWidget *parent = 0);
    ~Application();

private:
    Ui::Application *m_ui;

    KeybindingsModel m_keybindings;
    MousebindingsModel m_mousebindings;

    std::unique_ptr<ApplicationMode> m_curr_mode;
    std::unordered_map<QMdiSubWindow*, QMetaObject::Connection> m_mdi_connections;

private:
    void subdialog_done(QMdiSubWindow *wnd);
    void mdi_window_closed();
    void mdi_window_opened();

protected:
    void resizeEvent(QResizeEvent *event) override;

public:
    void enter_mode(std::unique_ptr<ApplicationMode> &&mode);
    OpenGLScene &scene();
    void show_dialog(QDialog &window);
    void show_preferences_dialog();
    void show_widget_as_window(QWidget &window, Qt::WindowFlags flags = 0);
    void quit();

    inline Ui::Application &ui()
    {
        return *m_ui;
    }

    inline KeybindingsModel &keybindings()
    {
        return m_keybindings;
    }

    inline MousebindingsModel &mousebindings()
    {
        return m_mousebindings;
    }

};

#endif // APPLICATION_HPP
