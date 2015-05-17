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
#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "fixups.hpp"

#include <memory>

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#include "ffengine/io/filesystem.hpp"

#include "quickglscene.hpp"
#include "mode.hpp"


class Application: public QQuickWindow
{
    Q_OBJECT
public:
    enum Mode {
        MAIN_MENU,
        TERRAFORM,
    };

public:
    Application();
    ~Application();

private:
    std::unique_ptr<ApplicationMode> m_curr_mode;
    QuickGLScene *m_gl_scene;

protected:
    QQmlEngine &engine();
    ApplicationMode &ensure_mode();
    QuickGLScene &ensure_scene();
    void enter_mode(std::unique_ptr<ApplicationMode> &&mode);

signals:

public slots:

public:
    inline QuickGLScene &scene()
    {
        return ensure_scene();
    }

    void enter_mode(Mode mode);

};

#endif // APPLICATION_HPP
