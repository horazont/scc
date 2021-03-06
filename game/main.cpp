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
#include <QResource>
#include <QSettings>
#include <QStyleFactory>

#include "application.hpp"
#include "mainmenu.hpp"
#include "terraform/terraform.hpp"

#include "ffengine/io/log.hpp"


static const QString organization = QStringLiteral("zombofant.net");
static const QString application = QStringLiteral("scc");


int main(int argc, char *argv[])
{
    QSettings pre_app_settings(organization, application);
    pre_app_settings.beginGroup("gl");
    pre_app_settings.beginGroup("surface");

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(pre_app_settings.value("samples", 0).toUInt());
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(0);
    format.setStencilBufferSize(pre_app_settings.value("stencil", 8).toUInt());
    format.setDepthBufferSize(pre_app_settings.value("depth", 24).toUInt());

    QSurfaceFormat::setDefaultFormat(format);

    io::logging().attach_sink<io::LogAsynchronousSink>(
                std::move(std::make_unique<io::LogTTYSink>())
                )->set_synchronous(true);
    io::logging().log(io::LOG_INFO) << "Log initialized" << io::submit;

    io::logging().get_logger("gl.array").set_level(io::LOG_WARNING);
    io::logging().get_logger("render.quadterrain").set_level(io::LOG_WARNING);
    io::logging().get_logger("gl.vao").set_level(io::LOG_WARNING);
    io::logging().get_logger("gl.shader").set_level(io::LOG_INFO);
    io::logging().get_logger("gl.debug").set_level(io::LOG_DEBUG);
    io::logging().get_logger("render.scenegraph").set_level(io::LOG_WARNING);
    io::logging().get_logger("render.camera").set_level(io::LOG_WARNING);

    QApplication qapp(argc, argv);
    qapp.setStyle(QStyleFactory::create("fusion"));
    io::logging().log(io::LOG_INFO) << "QApplication initialized" << io::submit;

    QResource::registerResource("textures.rcc");
    QResource::registerResource("shaders.rcc");
    QResource::registerResource("brushes.rcc");
    QResource::registerResource("stylesheets.rcc");
    io::logging().log(io::LOG_INFO) << "Resource packs registered" << io::submit;

    {
        QFile css_file(":/stylesheets/main.css");;
        if (!css_file.open(QIODevice::ReadOnly)) {
            io::logging().logf(io::LOG_ERROR, "failed to load stylesheet");
        } else {
            qapp.setStyleSheet(css_file.readAll());
        }
    }

    Application app;
    app.show();
    io::logging().log(io::LOG_INFO) << "Main window created" << io::submit;
    app.enter_mode(Application::MAIN_MENU);
    io::logging().log(io::LOG_INFO) << "Ready to roll out!" << io::submit;

    int exitcode = qapp.exec();
    io::logging().log(io::LOG_INFO) << "Terminated. Exit code: " << exitcode
                                    << io::submit;
    return exitcode;
}
