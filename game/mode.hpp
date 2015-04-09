#ifndef SCC_MODE_H
#define SCC_MODE_H

#include "fixups.hpp"

#include <QQuickItem>

#include "quickglscene.hpp"


class Application;

class ApplicationMode: public QQuickItem
{
    Q_OBJECT
public:
    ApplicationMode(const std::string &qmlname,
                    QQmlEngine *engine,
                    const QUrl &url);

public:
    virtual ~ApplicationMode();

private:
    const std::string m_qmlname;
    QQmlEngine *m_engine;
    QQmlComponent m_loader;
    bool m_loaded;

protected:
    Application *m_app;
    QuickGLScene *m_gl_scene;

protected:
    void load();

public:
    virtual void activate(Application &app,
                          QQuickItem &parent);
    virtual void deactivate();
};

void component_error(QQmlComponent *comp);

void component_ready(QQmlComponent *comp);

#endif
