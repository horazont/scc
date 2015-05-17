/**********************************************************************
File name: mode.cpp
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
#include "mode.hpp"

#include <QQmlContext>
#include <QQmlProperty>
#include <QQmlExpression>

#include "ffengine/io/log.hpp"
static io::Logger &app_logger = io::logging().get_logger("app");

#include "application.hpp"


ApplicationMode::ApplicationMode(const std::string &qmlname,
                                 QQmlEngine *engine,
                                 const QUrl &url):
    m_qmlname(qmlname),
    m_engine(engine),
    m_loader(engine, url),
    m_loaded(false),
    m_app(nullptr),
    m_gl_scene(nullptr)
{
}

ApplicationMode::~ApplicationMode()
{

}

void ApplicationMode::load()
{
    switch (m_loader.status()) {
    case QQmlComponent::Null:
    case QQmlComponent::Loading:
    {
        app_logger.log(io::LOG_EXCEPTION)
                << "unexpected QML component status ("
                << m_loader.status()
                << ")"
                << io::submit;
        return;
    }
    case QQmlComponent::Ready:
    {
        component_ready(&m_loader);
        break;
    }
    case QQmlComponent::Error:
    {
        component_error(&m_loader);
        return;
    }
    }

    m_engine->rootContext()->setContextProperty(QString(m_qmlname.data()),
                                                this);
    QQuickItem *item = qobject_cast<QQuickItem*>(m_loader.create());
    item->setParentItem(this);
    m_loaded = true;
}

void ApplicationMode::activate(Application &app, QQuickItem &parent)
{
    m_app = &app;
    if (!m_loaded) {
        load();
    }
    setParentItem(&parent);
    QQmlExpression expr(m_engine->rootContext(), this, "parent");
    QQmlProperty prop(this, "anchors.fill");
    prop.write(expr.evaluate());

    m_gl_scene = &app.scene();
}

void ApplicationMode::deactivate()
{
    m_gl_scene = nullptr;
    setParentItem(nullptr);
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


void component_ready(QQmlComponent*)
{
    app_logger.log(io::LOG_INFO, "QML component loaded");
}
