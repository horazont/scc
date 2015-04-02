#include "mode.hpp"

#include "engine/io/log.hpp"
static io::Logger &app_logger = io::logging().get_logger("app");


ApplicationMode::ApplicationMode():
    m_app(nullptr)
{

}

ApplicationMode::~ApplicationMode()
{

}

void ApplicationMode::activate(Application &app, QQuickItem &parent)
{
    m_app = &app;
}

void ApplicationMode::deactivate()
{
    m_app = nullptr;
}


void component_error(QQmlComponent *comp)
{
    app_logger.log(io::LOG_EXCEPTION, "QML component failed to load");
    for (auto &err: comp->errors()) {
        app_logger.log(io::LOG_ERROR)
                << err.url().toString().toStdString()
                << ":"
                << err.line()
                << ":"
                << err.column()
                << ": "
                << err.description().toStdString()
                << io::submit;
    }
}


void component_ready(QQmlComponent *comp)
{
    app_logger.log(io::LOG_INFO, "QML component loaded");
}
