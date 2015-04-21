import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Controls.Styles 1.3
import SCC 1.0

Application {
    title: qsTr("Hello World")
    width: 1067
    height: 600
    visible: true

    GLScene {
        objectName: "glscene";
    }

}
