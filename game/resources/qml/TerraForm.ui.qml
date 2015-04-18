import QtQuick 2.4
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtQuick.Layouts 1.0

Item {
    id: item1
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.right: parent.right

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
        width: 48
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: -4;
        anchors.rightMargin: -4;
        anchors.bottomMargin: -4;

        RowLayout {
            id: rowLayout1
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.top: parent.top

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                id: tool_raise
                objectName: "tool_raise"
                implicitWidth: 48
                implicitHeight: 48
                text: "Rse"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
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
                anchors.top: parent.top
                anchors.bottom: parent.bottom
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
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_flatten()
            }

            Item {
                Layout.fillWidth: true
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

    Component {
        id: mySliderStyle

        SliderStyle {
            groove: Item {
                width: control.width
                height: control.height

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0, 1, 0, 1);
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    height: 6
                    width: styleData.handlePosition
                    color: Qt.rgba(0, 0, 1, 1);
                }
            }
            handle: Rectangle {
                x: styleData.handlePosition
                width: 10
                height: 10
                color: Qt.rgba(0, 0, 1, 1);
            }
        }
    }


    Item {
        id: brushBoxItem
        anchors.bottom: tools1.top
        anchors.left: tools1.left
        anchors.leftMargin: 8;
        height: 200
        width: 200

        Item {
            anchors.fill: parent;

            GridLayout {
                id: grid
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                columns: 2

                Label {
                    text: qsTr("<b>Brush settings</b>")

                    Layout.row: 1
                    Layout.column: 1
                    Layout.columnSpan: 2
                }

                Label {
                    id: label1
                    text: qsTr("Brush size")

                    Layout.row: 2
                    Layout.column: 1
                }

                Label {
                    id: brushSizeValueLabel
                    text: brushSizeSlider.value

                    horizontalAlignment: Text.AlignRight

                    Layout.alignment: Qt.AlignRight

                    Layout.row: 2
                    Layout.column: 2

                    Layout.minimumWidth: 40
                }

                Slider {
                    id: brushSizeSlider
                    minimumValue: 1
                    maximumValue: 1024
                    stepSize: 1

                    Layout.row: 3
                    Layout.column: 1
                    Layout.columnSpan: 2

                    /* Layout.minimumWidth: 0
                    Layout.preferredWidth: 150
                    Layout.maximumWidth: 10000 */
                    Layout.fillWidth: true

                    value: 32

                    onValueChanged: Terraform.set_brush_size(value)
                }

                Label {
                    id: label2
                    text: qsTr("Brush strength")

                    Layout.row: 4
                    Layout.column: 1
                }

                Label {
                    id: brushStrengthValueLabel
                    text: brushStrengthSlider.value

                    horizontalAlignment: Text.AlignRight

                    Layout.row: 4
                    Layout.column: 2

                    Layout.alignment: Qt.AlignRight

                    Layout.minimumWidth: 40
                }

                Slider {
                    id: brushStrengthSlider

                    Layout.row: 5
                    Layout.column: 1
                    Layout.columnSpan: 2

                    /* Layout.minimumWidth: 0
                    Layout.preferredWidth: 150
                    Layout.maximumWidth: 10000 */
                    Layout.fillWidth: true

                    minimumValue: 0
                    maximumValue: 1
                    stepSize: 0.01

                    value: 1.0

                    onValueChanged: Terraform.set_brush_strength(value)
                }
            }
        }
    }
}

