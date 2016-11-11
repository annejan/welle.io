import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2

// Import custom styles
import "."

Row {
    property alias name: nameView.text
    property alias objectName: switchView.objectName
    property alias checked: switchView.checked

    spacing: Units.dp(20)
    Text {
        id: nameView
        width: Units.dp(212)
        font.pixelSize: Style.textStandartSize
        font.family: Style.textFont
        color: Style.textColor
        Behavior on x { NumberAnimation{ easing.type: Easing.OutCubic} }
    }
    Switch {
        id: switchView
        style: switchStyle
    }

    /* Switch Style */
    Component {
        id: switchStyle
        SwitchStyle {

            groove: Rectangle {
                implicitHeight: Units.dp(25)
                implicitWidth: Units.dp(70)
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    width: parent.width/2 - Units.dp(2)
                    height: Units.dp(20)
                    anchors.margins: Units.dp(2)
                    color: control.checked ? "#468bb7" : "#222"
                    Behavior on color {ColorAnimation {}}
                    Text {
                        font.pixelSize: Units.em(1.1)
                        color: "white"
                        font.family: Style.textFont
                        anchors.centerIn: parent
                        text: "ON"
                    }
                }
                Item {
                    width: parent.width/2
                    height: parent.height
                    anchors.right: parent.right
                    Text {
                        font.pixelSize: Units.em(1.1)
                        color: "white"
                        font.family: Style.textFont
                        anchors.centerIn: parent
                        text: "OFF"
                    }
                }
                color: "#222"
                border.color: "#444"
                border.width: Units.dp(2)
            }
            handle: Rectangle {
                width: parent.parent.width/2
                height: control.height
                color: "#444"
                border.color: "#555"
                border.width: Units.dp(2)
            }
        }
    }
}