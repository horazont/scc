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

#include <QColorDialog>
#include <QDialog>
#include <QMdiSubWindow>
#include <QResizeEvent>


static io::Logger &logger = io::logging().get_logger("app");

Application::Application(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::Application)
{
    m_ui->setupUi(this);
    show_dialog(*(new QColorDialog(this)));
}

Application::~Application()
{
    if (m_curr_mode) {
        m_curr_mode->deactivate();
    }
    delete m_ui;
}

void Application::subdialog_done(QMdiSubWindow *wnd)
{
    m_ui->mdiArea->removeSubWindow(wnd);
    auto iter = m_mdi_connections.find(wnd);
    wnd->widget()->disconnect(iter->second);
    m_mdi_connections.erase(iter);

    mdi_window_closed();
}

void Application::mdi_window_closed()
{
    if (m_ui->mdiArea->subWindowList().size() == 0) {
        m_ui->mdiArea->hide();
    }
}

void Application::mdi_window_opened()
{
    m_ui->mdiArea->show();
}

void Application::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    QRect geometry = m_ui->centralwidget->geometry();
    m_ui->sceneWidget->setGeometry(geometry);
    m_ui->modeParent->setGeometry(geometry);
    if (m_curr_mode) {
        m_curr_mode->setGeometry(geometry);
    }
    m_ui->mdiArea->setGeometry(geometry);
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
        m_curr_mode->activate(*this, *m_ui->modeParent);
    }
}

OpenGLScene &Application::scene()
{
    return *m_ui->sceneWidget;
}

void Application::show_dialog(QDialog &window)
{
    QMdiSubWindow *wnd = m_ui->mdiArea->addSubWindow(&window, window.windowFlags());
    QMetaObject::Connection conn = connect(&window, &QDialog::finished,
                                           this, [this, wnd](int){ subdialog_done(wnd); });
    m_mdi_connections[wnd] = conn;
    window.open();
    mdi_window_opened();
}
