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
#include <QMainWindow>

#include "ffengine/io/filesystem.hpp"

#include "mode.hpp"
#include "openglscene.hpp"


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

    std::unique_ptr<ApplicationMode> m_curr_mode;

protected:
    void resizeEvent(QResizeEvent *event) override;

public:
    void enter_mode(std::unique_ptr<ApplicationMode> &&mode);
    OpenGLScene &scene();

};

#endif // APPLICATION_HPP
