/**********************************************************************
File name: mode.hpp
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
#ifndef SCC_MODE_H
#define SCC_MODE_H

#include "fixups.hpp"

#include <QQuickItem>

#include "quickglscene.hpp"


class Application;

class ApplicationMode: public QQuickItem
{
    Q_OBJECT
public:
    ApplicationMode(const std::string &qmlname,
                    QQmlEngine *engine,
                    const QUrl &url);

public:
    virtual ~ApplicationMode();

private:
    const std::string m_qmlname;
    QQmlEngine *m_engine;
    QQmlComponent m_loader;
    bool m_loaded;

protected:
    Application *m_app;
    QuickGLScene *m_gl_scene;

protected:
    void load();

public:
    virtual void activate(Application &app,
                          QQuickItem &parent);
    virtual void deactivate();
};

void component_error(QQmlComponent *comp);

void component_ready(QQmlComponent *comp);

#endif
