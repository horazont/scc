#include "application.hpp"

#include <QQmlContext>

#include "engine/io/mount.hpp"
#include "engine/io/log.hpp"

#include "mainmenu.hpp"
#include "terraform.hpp"

static io::Logger &app_logger = io::logging().get_logger("app");


Application::Application():
    QQuickWindow(),
    m_curr_mode(nullptr),
    m_gl_scene(nullptr)
{
    m_vfs.mount("/resources",
                std::unique_ptr<io::MountDirectory>(
                    new io::MountDirectory("resources/", true)),
                io::MountPriority::FileSystem);
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
