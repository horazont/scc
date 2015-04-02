#ifndef SCC_MODE_H
#define SCC_MODE_H

#include <GL/glew.h>

#include <QQuickItem>

class Application;

class ApplicationMode: public QObject
{
    Q_OBJECT
public:
    ApplicationMode();

public:
    virtual ~ApplicationMode();

protected:
    Application *m_app;

public:
    virtual void activate(Application &app,
                          QQuickItem &parent);
    virtual void deactivate();
};

void component_error(QQmlComponent *comp);

void component_ready(QQmlComponent *comp);

#endif
