#ifndef SCC_MAINMENU_H
#define SCC_MAINMENU_H

#include <GL/glew.h>

#include "engine/common/resource.hpp"
#include "engine/render/scenegraph.hpp"

#include "mode.hpp"

class QuickGLScene;


struct MainMenuScene
{
    engine::ResourceManager m_resources;
    engine::SceneGraph m_scenegraph;
    engine::PerspectivalCamera m_camera;
};


class MainMenu: public ApplicationMode
{
    Q_OBJECT
public:
    MainMenu(QQmlEngine *engine);
    ~MainMenu() override;

private:
    std::unique_ptr<MainMenuScene> m_scene;

    QMetaObject::Connection m_gl_sync_conn;

protected:
    void prepare_scene();

signals:

public slots:
    void before_gl_sync();

public:
    void activate(Application &app, QQuickItem &parent) override;
    void deactivate() override;


};

#endif
