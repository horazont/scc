/**********************************************************************
File name: mode.cpp
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
#include "mode.hpp"

#include "application.hpp"


ApplicationMode::ApplicationMode(Application &app, QWidget *parent):
    QWidget(parent),
    m_app(app),
    m_gl_scene(nullptr)
{
}

ApplicationMode::~ApplicationMode()
{

}

void ApplicationMode::activate(QWidget &parent)
{
    m_gl_scene = &m_app.scene();
    setParent(&parent);
    setGeometry(0, 0, parent.width(), parent.height());
    setVisible(true);
}

void ApplicationMode::deactivate()
{
    setParent(nullptr);
    m_gl_scene = nullptr;
}
