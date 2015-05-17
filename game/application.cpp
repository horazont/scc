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

#include <QQmlContext>

#include "ffengine/io/mount.hpp"
#include "ffengine/io/log.hpp"

#include "mainmenu.hpp"
#include "terraform/terraform.hpp"

static io::Logger &app_logger = io::logging().get_logger("app");


Application::Application():
    QQuickWindow(),
    m_curr_mode(nullptr),
    m_gl_scene(nullptr)
{

}

Application::~Application()
{

}

QQmlEngine &Application::engine()
{
    QQmlEngine *result = QQmlEngine::contextForObject(this)->engine();
    if (!result) {
        app_logger.log(io::LOG_EXCEPTION,
                       "no engine associated with "
                       "application");
        throw std::runtime_error("no engine for application");
    }

    return *result;
}

ApplicationMode &Application::ensure_mode()
{
    if (!m_curr_mode) {
        m_curr_mode = std::unique_ptr<ApplicationMode>(
                    new TerraformMode(&engine())
                    );
        app_logger.log(io::LOG_DEBUG, "activating mode");
        m_curr_mode->activate(*this, *contentItem());
        app_logger.log(io::LOG_DEBUG, "mode activated");
    }
    return *m_curr_mode;
}

QuickGLScene &Application::ensure_scene()
{
    if (!m_gl_scene) {
        m_gl_scene = findChild<QuickGLScene*>("glscene");
        if (!m_gl_scene) {
            app_logger.log(io::LOG_EXCEPTION,
                           "no glscene object in application");
            throw std::runtime_error("no glscene object in application");
        }
    }

    return *m_gl_scene;
}

void Application::enter_mode(std::unique_ptr<ApplicationMode> &&mode)
{
    if (m_curr_mode) {
        m_curr_mode->deactivate();
    }
    m_curr_mode = std::move(mode);
    if (!m_curr_mode) {
        ensure_mode();
    } else {
        m_curr_mode->activate(*this, *contentItem());
    }
}

void Application::enter_mode(Mode mode)
{
    switch (mode) {
    case TERRAFORM:
    {
        enter_mode(std::unique_ptr<ApplicationMode>(new TerraformMode(&engine())));
        return;
    }
    case MAIN_MENU:
    {
        enter_mode(std::unique_ptr<ApplicationMode>(new MainMenu(&engine())));
        return;
    }
    }
}
