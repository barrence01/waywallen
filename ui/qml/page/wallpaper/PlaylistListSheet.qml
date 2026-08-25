pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Qcm.Material as MD
import waywallen.ui as W

MD.BottomSheet {
    id: control

    required property Item popupParent
    required property var sheetState

    parent: popupParent
    anchors.fill: parent
    z: 30
    sheetType: MD.Enum.BottomSheetModal
    dismissOnDragDown: true
    maxSheetWidth: 560

    MD.Action {
        id: createPlaylistAction
        text: qsTr("Create playlist")
        icon.name: MD.Token.icon.playlist_add
        displayHint: MD.ToolBarLayout.KeepVisible
        enabled: !control.sheetState.mutationQuerying
        busy: control.sheetState.createQuerying ? MD.Enum.Busy : MD.Enum.Idle
        onTriggered: control.sheetState.createPlaylist()
    }

    ColumnLayout {
        width: control.sheetWidth
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 8
            implicitHeight: titleText.implicitHeight

            MD.Text {
                id: titleText

                anchors.left: parent.left
                anchors.right: titleActions.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Playlists")
                typescale: MD.Token.typescale.title_medium
                color: MD.Token.color.on_surface
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            MD.ActionToolBar {
                id: titleActions

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                actions: [createPlaylistAction]
                iconDelegate: MD.BusyIconButton {
                    action: MD.ToolBarLayout.action
                }
            }
        }

        W.PresentationTargetFlow {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 8
            enabled: !control.sheetState.playbackQuerying
            targetState: control.sheetState.targetState
            allToolTip: qsTr("Includes displays connected later")
        }

        MD.LinearIndicator {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 8
            visible: control.sheetState.listLoading
            running: visible
        }

        MD.Text {
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            visible: !control.sheetState.listLoading && control.sheetState.playlists.length === 0
            text: qsTr("No playlists found")
            typescale: MD.Token.typescale.body_large
            color: MD.Token.color.on_surface_variant
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MD.VerticalListView {
            id: playlistSheetList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, Math.max(120, contentHeight + topMargin + bottomMargin))
            visible: control.sheetState.playlists.length > 0
            interactive: contentHeight + topMargin + bottomMargin > height
            model: control.sheetState.playlists
            spacing: 6
            leftMargin: 16
            rightMargin: 16
            topMargin: 0
            bottomMargin: 16

            delegate: MD.ListItem {
                id: playlistSheetItem
                required property var modelData

                width: ListView.view.contentWidth
                radius: 12
                text: modelData.name || qsTr("Untitled")
                supportText: qsTr("%n wallpaper(s)", "", (modelData.entryIds || []).length)
                heightMode: playingDisplayLabels.length > 0 ? MD.Enum.ListItemThreeLine : MD.Enum.ListItemTwoLine
                readonly property bool playingOnSelectedTargets: control.sheetState.playlistIsPlayingOnSelectedTargets(modelData)
                readonly property var playingDisplayLabels: control.sheetState.playlistDisplayLabels(modelData)
                mdState.backgroundColor: MD.Token.color.surface_container

                below: Item {
                    implicitHeight: tagFlow.visible ? tagFlow.implicitHeight + 6 : 0

                    Flow {
                        id: tagFlow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        spacing: 4
                        visible: playlistSheetItem.playingDisplayLabels.length > 0

                        Repeater {
                            model: playlistSheetItem.playingDisplayLabels

                            W.Tag {
                                required property var modelData
                                text: modelData
                                bgColor: MD.Token.color.secondary_container
                                fgColor: MD.Token.color.on_secondary_container
                            }
                        }
                    }
                }

                trailing: RowLayout {
                    spacing: 4

                    MD.BusyIconButton {
                        enabled: control.sheetState.hasPlayTarget && !control.sheetState.playlistPlaybackMutation.querying
                        busy: control.sheetState.playlistPlaybackMutation.querying
                        icon.name: playlistSheetItem.playingOnSelectedTargets ? MD.Token.icon.pause : MD.Token.icon.play_arrow
                        onClicked: control.sheetState.togglePlayback(playlistSheetItem.modelData)
                        MD.ToolTip.visible: hovered && !control.sheetState.playbackQuerying
                        MD.ToolTip.text: !control.sheetState.hasPlayTarget ? qsTr("No displays") : playlistSheetItem.playingOnSelectedTargets ? qsTr("Pause playlist") : qsTr("Play playlist")
                    }

                    MD.IconButton {
                        enabled: !control.sheetState.mutationQuerying
                        icon.name: MD.Token.icon.edit
                        onClicked: control.sheetState.editPlaylist(playlistSheetItem.modelData)
                        MD.ToolTip.visible: hovered
                        MD.ToolTip.text: qsTr("Edit playlist")
                    }

                    MD.IconButton {
                        enabled: !control.sheetState.mutationQuerying
                        icon.name: MD.Token.icon.edit_note
                        onClicked: control.sheetState.editPlaylistItems(playlistSheetItem.modelData)
                        MD.ToolTip.visible: hovered
                        MD.ToolTip.text: qsTr("Edit wallpapers")
                    }

                    MD.IconButton {
                        enabled: !control.sheetState.mutationQuerying
                        icon.name: MD.Token.icon.delete
                        onClicked: control.sheetState.deletePlaylist(playlistSheetItem.modelData)
                        MD.ToolTip.visible: hovered
                        MD.ToolTip.text: qsTr("Delete playlist")
                    }
                }
            }
        }
    }
}
