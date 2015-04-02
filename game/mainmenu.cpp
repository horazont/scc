#include "mainmenu.hpp"

#include "engine/io/log.hpp"
static io::Logger &app_logger = io::logging().get_logger("app");


#include "application.hpp"
#include "quickglscene.hpp"


MainMenu::MainMenu(QQmlEngine *engine):
    m_loader(engine, QUrl("qrc:/qml/MainMenu.qml"))
{

}

MainMenu::~MainMenu()
{
    if (m_item) {
        delete m_item;
    }
}

void MainMenu::load()
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

    m_item = qobject_cast<QQuickItem*>(m_loader.create());
}

void MainMenu::prepare_scene()
{
    if (m_scene) {
        return;
    }

    m_scene = std::unique_ptr<MainMenuScene>(new MainMenuScene());
}

void MainMenu::before_gl_sync()
{
    prepare_scene();
    m_gl_scene->setup_scene(m_scene->m_scenegraph,
                            m_scene->m_camera);
}

void MainMenu::activate(Application &app, QQuickItem &parent)
{
    ApplicationMode::activate(app, parent);

    if (!m_item) {
        load();
    }

    m_item->setParentItem(&parent);
    m_gl_scene = &app.scene();

    m_gl_sync_conn = connect(
                m_gl_scene, &QuickGLScene::before_gl_sync,
                this, &MainMenu::before_gl_sync,
                Qt::DirectConnection);
}

void MainMenu::deactivate()
{
    disconnect(m_gl_sync_conn);
    ApplicationMode::deactivate();
}
