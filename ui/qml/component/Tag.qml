pragma ComponentBehavior: Bound
import QtQuick
import Qcm.Material as MD

// Compact pill-shaped label chip. Defaults to the primary container color;
// callers override bgColor/fgColor (e.g. a per-vendor MdColorMgr scheme).
Rectangle {
    id: root

    property alias text: tagText.text
    property alias textItem: tagText
    property color bgColor: MD.Token.color.primary_container
    property color fgColor: MD.Token.color.on_primary_container
    property bool removable: false
    signal removed()

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight
    radius: height / 2
    color: root.bgColor

    Row {
        id: content
        spacing: 0
        leftPadding: 8
        rightPadding: root.removable ? 2 : 8
        topPadding: 3
        bottomPadding: 3

        MD.Text {
            id: tagText
            anchors.verticalCenter: parent.verticalCenter
            typescale: MD.Token.typescale.label_small
            color: root.fgColor
        }

        MD.SmallIconButton {
            visible: root.removable
            width: visible ? implicitWidth : 0
            icon.name: MD.Token.icon.close
            Accessible.name: qsTr("Remove")
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.removed()
        }
    }
}
