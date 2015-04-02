#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <GL/glew.h>

#include <memory>

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#include <quickglscene.hpp>

#include "mode.hpp"


class Application: public QQuickWindow
{
    Q_OBJECT
public:
    Application();
    ~Application();

private:
    std::unique_ptr<ApplicationMode> m_curr_mode;
    QuickGLScene *m_gl_scene;

protected:
    ApplicationMode &ensure_mode();
    QuickGLScene &ensure_scene();

signals:

public slots:

public:
    inline QuickGLScene &scene()
    {
        return ensure_scene();
    }

};

#endif // APPLICATION_HPP
