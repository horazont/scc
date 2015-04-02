import QtQuick 2.4
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtQuick.Layouts 1.0

Item {
    id: item1
    width: 48
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.bottom: parent.bottom

    Component {
        id: myToolStyle;

        ButtonStyle {
            background: Rectangle {
                color: (control.checked ? Qt.rgba(0.3, 0.4, 0.5, 0.8) : "transparent")
                border.color: Qt.rgba(0.3, 0.4, 0.5, 0.5)
                border.width: (control.hovered ? 2 : 0)
            }
        }
    }

    ToolBar {
        id: tools1
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: -4;
        anchors.topMargin: -4;
        anchors.bottomMargin: -4;

        ColumnLayout {
            id: columnLayout1
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.top: parent.top

            Item {
                Layout.fillHeight: true
            }

            ToolButton {
                id: tool_raise
                objectName: "tool_raise"
                implicitWidth: 48
                implicitHeight: 48
                text: "Rse"
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_raise()
            }

            ToolButton {
                id: tool_lower
                objectName: "tool_lower"
                implicitWidth: 48
                implicitHeight: 48
                text: "Low"
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_lower()
            }

            ToolButton {
                id: tool_flatten
                objectName: "tool_flatten"
                implicitWidth: 48
                implicitHeight: 48
                text: "Flt"
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_flatten()
            }

            Item {
                Layout.fillHeight: true
            }
        }

        style: ToolBarStyle {
            background: Rectangle {
                color: Qt.rgba(1.0, 1.0, 1.0, 0.8);
                border.color: Qt.rgba(0, 0, 0, 0.5);
                border.width: 4;
            }
        }
    }
}

