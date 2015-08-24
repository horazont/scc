/**********************************************************************
File name: application.cpp
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
#include "application.hpp"
#include "ui_application.h"

#include <QResizeEvent>

static io::Logger &logger = io::logging().get_logger("app");

Application::Application(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::Application)
{
    m_ui->setupUi(this);
}

Application::~Application()
{
    if (m_curr_mode) {
        m_curr_mode->deactivate();
    }
    delete m_ui;
}

void Application::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    QRect geometry = m_ui->centralwidget->geometry();
    m_ui->sceneWidget->setGeometry(geometry);
    if (m_curr_mode) {
        m_curr_mode->setGeometry(geometry);
    }
}

void Application::enter_mode(std::unique_ptr<ApplicationMode> &&mode)
{
    if (m_curr_mode) {
        logger.log(io::LOG_DEBUG, "deactivating previous mode");
        m_curr_mode->deactivate();
    }
    m_curr_mode = std::move(mode);
    if (m_curr_mode) {
        logger.log(io::LOG_DEBUG, "activating new mode");
        m_curr_mode->activate(*this, *m_ui->centralwidget);
    }
}

OpenGLScene &Application::scene()
{
    return *m_ui->sceneWidget;
}
