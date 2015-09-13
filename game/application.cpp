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

#include "preferencesdialog.hpp"

#include "mainmenu.hpp"
#include "terraform/terraform.hpp"


static io::Logger &logger = io::logging().get_logger("app");

Application::Application(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::Application),
    m_main_menu(new MainMenu(*this)),
    m_map_editor(new TerraformMode(*this)),
    m_curr_mode(nullptr)
{
    m_ui->setupUi(this);
    m_ui->mdiArea->hide();
}

Application::~Application()
{
    if (m_curr_mode) {
        enter_mode(nullptr);
    }
    delete m_ui;
}

void Application::enter_mode(ApplicationMode *mode)
{
    if (m_curr_mode) {
        logger.log(io::LOG_DEBUG, "deactivating previous mode");
        m_curr_mode->deactivate();
    }
    m_curr_mode = mode;
    if (m_curr_mode) {
        logger.log(io::LOG_DEBUG, "activating new mode");
        m_curr_mode->activate(*m_ui->modeParent);
    }
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
        if (m_curr_mode) {
            m_curr_mode->setFocus();
        }
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

void Application::enter_mode(Mode mode)
{
    switch (mode)
    {
    case MAIN_MENU:
    {
        enter_mode(m_main_menu.get());
        break;
    }
    case TERRAFORM:
    {
        enter_mode(m_map_editor.get());
        break;
    }
    }
}

OpenGLScene &Application::scene()
{
    return *m_ui->sceneWidget;
}

void Application::show_dialog(QDialog &window)
{
    const unsigned int w = window.width();
    const unsigned int h = window.height();

    const unsigned int x = std::round(float(width()) / 2. - float(w) / 2.);
    const unsigned int y = std::round(float(height()) / 2. - float(h) / 2.);

    QMdiSubWindow *wnd = m_ui->mdiArea->addSubWindow(&window, window.windowFlags());
    wnd->setGeometry(x, y, w, h);
    QMetaObject::Connection conn = connect(&window, &QDialog::finished,
                                           this, [this, wnd](int){ subdialog_done(wnd); });
    m_mdi_connections[wnd] = conn;
    window.open();
    mdi_window_opened();
}

void Application::show_preferences_dialog()
{
    show_dialog(*(new PreferencesDialog(m_keybindings, m_mousebindings)));
}

void Application::quit()
{
    close();
}
