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
                objectName: "tool_raise_lower"
                implicitWidth: 48
                implicitHeight: 48
                text: "R/L"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_raise_lower()
            }

            ToolButton {
                id: tool_flatten
                objectName: "tool_flatten"
                implicitWidth: 48
                implicitHeight: 48
                text: "Lvl"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_flatten()
            }

            ToolButton {
                id: tool_smooth
                objectName: "tool_smooth"
                implicitWidth: 48
                implicitHeight: 48
                text: "Smth"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_smooth()
            }

            ToolButton {
                id: tool_fluid_raise
                objectName: "tool_fluid_raise"
                implicitWidth: 48
                implicitHeight: 48
                text: "FR/L"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 0
                style: myToolStyle
                onClicked: Terraform.switch_to_tool_fluid_raise()
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
        anchors.leftMargin: 4;
        height: 300
        width: 200

        Rectangle {
            anchors.fill: parent;

            color: Qt.rgba(1, 1, 1, 0.8);

            GridLayout {
                id: grid
                anchors.fill: parent
                anchors.margins: 4;
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

                Label {
                    Layout.row: 6
                    Layout.column: 1

                    text: qsTr("Brush type")
                }

                Label {
                    Layout.row: 6
                    Layout.column: 2

                    Layout.fillWidth: true

                    text: brushList.currentItem.name
                }

                GridView {
                    id: brushList

                    Layout.row: 7
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Layout.column: 1
                    Layout.columnSpan: 2

                    cellHeight: 34
                    cellWidth: 34

                    clip: true

                    model: Terraform.brush_list_model

                    focus: true

                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(1, 1, 1, 1)
                        z: -1
                    }

                    delegate: MouseArea {
                        height: 34
                        width: 34

                        property string name
                        name: display_name

                        Image {
                            anchors.fill: parent;
                            anchors.margins: 1;
                            source: image_url
                            sourceSize: "32x32"
                        }
                        onClicked: brushList.currentIndex = index
                    }

                    highlight: Rectangle {
                        color: Qt.rgba(1, 0.9, 0.8, 1.0);
                    }
                    highlightFollowsCurrentItem: true
                    highlightMoveDuration: 0
                    onCurrentIndexChanged: Terraform.set_brush(currentIndex)
                }
            }
        }
    }
}

