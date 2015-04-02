TEMPLATE = app

QT += qml quick widgets

QMAKE_CXXFLAGS += -std=c++11
unix:LIBS += -lGLEW -lGLU

SOURCES += \
    main.cpp \
    application.cpp \
    quickglscene.cpp \
    mainmenu.cpp \
    mode.cpp \
    terraform.cpp

RESOURCES += qml.qrc

INCLUDEPATH += $$PWD/../engine/

LIBS += -L../engine -lengine

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

CONFIG += object_parallel_to_source

HEADERS += \
    application.hpp \
    quickglscene.hpp \
    mainmenu.hpp \
    mode.hpp \
    terraform.hpp

unix {
    copyfiles.commands = rm -rf ./resources; cp -r $$PWD/resources ./
}

QMAKE_EXTRA_TARGETS += copyfiles
POST_TARGETDEPS += copyfiles
