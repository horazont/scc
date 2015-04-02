#include "application.hpp"

#include <QQmlContext>

#include "engine/io/log.hpp"
static io::Logger &app_logger = io::logging().get_logger("app");

#include "mainmenu.hpp"


Application::Application()
{

}

Application::~Application()
{

}

ApplicationMode &Application::ensure_mode()
{
    if (!m_curr_mode) {
        m_curr_mode = std::unique_ptr<ApplicationMode>(
                    new MainMenu(QQmlEngine::contextForObject(this)->engine())
                    );
        m_curr_mode->activate(*this, *contentItem());
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
