#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <GL/glew.h>

#include <memory>

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#include "engine/io/filesystem.hpp"

#include "quickglscene.hpp"
#include "mode.hpp"


class Application: public QQuickWindow
{
    Q_OBJECT
public:
    enum Mode {
        MAIN_MENU,
        TERRAFORM,
    };

public:
    Application();
    ~Application();

private:
    std::unique_ptr<ApplicationMode> m_curr_mode;
    QuickGLScene *m_gl_scene;

protected:
    QQmlEngine &engine();
    ApplicationMode &ensure_mode();
    QuickGLScene &ensure_scene();
    void enter_mode(std::unique_ptr<ApplicationMode> &&mode);

signals:

public slots:

public:
    inline QuickGLScene &scene()
    {
        return ensure_scene();
    }

    void enter_mode(Mode mode);

};

#endif // APPLICATION_HPP
