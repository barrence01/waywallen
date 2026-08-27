pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Qcm.Material as MD
import waywallen.ui as W

ColumnLayout {
    id: root

    readonly property string githubUrl: "https://github.com/waywallen/waywallen-display"
    readonly property string kdeStoreUrl: "https://store.kde.org/p/2356221"

    spacing: 12
    visible: W.Notify.displayBackend.name === "kde-plasma"

    MD.Text {
        Layout.fillWidth: true
        text: qsTr("KDE Plasma needs the <b>waywallen-display</b> wallpaper extension to bridge wallpapers to the desktop. Install it from either source:")
        textFormat: Text.StyledText
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        typescale: MD.Token.typescale.body_medium
        color: MD.Token.color.on_surface
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 8

        MD.Button {
            text: qsTr("GitHub")
            mdState.type: MD.Enum.BtFilledTonal
            onClicked: MD.Util.openUrlExternally(root.githubUrl)
            MD.ToolTip.visible: hovered
            MD.ToolTip.text: root.githubUrl
        }
        MD.Button {
            text: qsTr("KDE Store")
            mdState.type: MD.Enum.BtFilledTonal
            onClicked: MD.Util.openUrlExternally(root.kdeStoreUrl)
            MD.ToolTip.visible: hovered
            MD.ToolTip.text: root.kdeStoreUrl
        }
    }

    MD.Text {
        Layout.fillWidth: true
        text: qsTr("Then right-click the desktop → <b>Configure Desktop and Wallpaper…</b> and pick the <b>Waywallen</b> wallpaper plugin.")
        textFormat: Text.StyledText
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        typescale: MD.Token.typescale.body_small
        color: MD.Token.color.on_surface_variant
    }
}
