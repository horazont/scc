#include <iostream>

#include <GL/glew.h>

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>

#include "quickglscene.hpp"
#include "application.hpp"

#include "engine/io/log.hpp"

void statusChanged(QQmlComponent *comp,
                   QQmlComponent::Status new_status)
{

}

int main(int argc, char *argv[])
{
    io::logging().attach_sink<io::LogAsynchronousSink>(
                std::move(std::unique_ptr<io::LogSink>(new io::LogTTYSink()))
                )->set_synchronous(true);
    io::logging().log(io::LOG_INFO) << "Log initialized" << io::submit;

    io::logging().get_logger("engine.gl.array").set_level(io::LOG_WARNING);
    io::logging().get_logger("engine.gl.vao").set_level(io::LOG_WARNING);
    io::logging().get_logger("engine.render.scenegraph").set_level(io::LOG_WARNING);
    io::logging().get_logger("engine.render.camera").set_level(io::LOG_WARNING);

    QApplication qapp(argc, argv);
    io::logging().log(io::LOG_INFO) << "QApplication initialized" << io::submit;

    qmlRegisterType<QuickGLScene>("SCC", 1, 0, "GLScene");
    io::logging().log(io::LOG_DEBUG) << "GLScene registered with QML" << io::submit;
    qmlRegisterType<Application>("SCC", 1, 0, "Application");
    io::logging().log(io::LOG_DEBUG) << "Application registered with QML" << io::submit;

    io::logging().log(io::LOG_INFO) << "QtQuick items registered" << io::submit;

    QQmlEngine engine;
    io::logging().log(io::LOG_INFO) << "QML engine initialized" << io::submit;
    QQmlComponent component(&engine, QUrl("qrc:/qml/main.qml"));

    Application *app = qobject_cast<Application*>(component.create());
    if (!app) {
        io::logging().log(io::LOG_EXCEPTION)
                << "Failed to get Application from QML scene!"
                << io::submit;
        return 1;
    }

    io::logging().log(io::LOG_INFO) << "QML scene created" << io::submit;

    io::logging().log(io::LOG_INFO) << "Ready to roll out!" << io::submit;
    int exitcode = qapp.exec();

    io::logging().log(io::LOG_INFO) << "Terminated. Exit code: " << exitcode
                                    << io::submit;
    return exitcode;
}
