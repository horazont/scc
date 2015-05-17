/**********************************************************************
File name: main.cpp
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
#include <iostream>

#include "fixups.hpp"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QResource>

#include "quickglscene.hpp"
#include "application.hpp"

#include "terraform/terraform.hpp"

#include "ffengine/io/log.hpp"

int main(int argc, char *argv[])
{
    io::logging().attach_sink<io::LogAsynchronousSink>(
                std::move(std::unique_ptr<io::LogSink>(new io::LogTTYSink()))
                )->set_synchronous(true);
    io::logging().log(io::LOG_INFO) << "Log initialized" << io::submit;

    io::logging().get_logger("gl.array").set_level(io::LOG_WARNING);
    io::logging().get_logger("render.quadterrain").set_level(io::LOG_WARNING);
    io::logging().get_logger("gl.vao").set_level(io::LOG_WARNING);
    io::logging().get_logger("render.scenegraph").set_level(io::LOG_WARNING);
    io::logging().get_logger("render.camera").set_level(io::LOG_WARNING);

    QApplication qapp(argc, argv);
    io::logging().log(io::LOG_INFO) << "QApplication initialized" << io::submit;

    qmlRegisterType<QuickGLScene>("SCC", 1, 0, "GLScene");
    io::logging().log(io::LOG_DEBUG) << "GLScene registered with QML" << io::submit;
    qmlRegisterType<Application>("SCC", 1, 0, "Application");
    io::logging().log(io::LOG_DEBUG) << "Application registered with QML" << io::submit;
    qmlRegisterType<BrushList>();
    io::logging().log(io::LOG_DEBUG) << "BrushList registered with QML" << io::submit;

    io::logging().log(io::LOG_INFO) << "QtQuick items registered" << io::submit;

    io::logging().log(io::LOG_INFO) << "Registering resource packs" << io::submit;
    QResource::registerResource("qml.rcc");
    QResource::registerResource("textures.rcc");
    QResource::registerResource("shaders.rcc");
    QResource::registerResource("brushes.rcc");

    QQmlEngine engine;
    engine.addImageProvider(BrushListImageProvider::provider_name, BrushList::image_provider());
    io::logging().log(io::LOG_INFO) << "QML engine initialized" << io::submit;
    QQmlComponent component(&engine, QUrl("qrc:/qml/main.qml"));

    Application *app = qobject_cast<Application*>(component.create());
    if (!app) {
        io::logging().log(io::LOG_EXCEPTION)
                << "Failed to get Application from QML scene!"
                << io::submit;
        return 1;
    }
    app->enter_mode(Application::TERRAFORM);

    io::logging().log(io::LOG_INFO) << "QML scene created" << io::submit;

    io::logging().log(io::LOG_INFO) << "Ready to roll out!" << io::submit;
    int exitcode = qapp.exec();

    io::logging().log(io::LOG_INFO) << "Terminated. Exit code: " << exitcode
                                    << io::submit;
    return exitcode;
}
