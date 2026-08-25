pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Qcm.Material as MD
import waywallen.ui as W

MD.Page {
    id: root

    required property double playlistId

    implicitWidth: 448
    implicitHeight: 600
    title: draftReady && draftName.length > 0 ? draftName : qsTr("Edit playlist")
    scrolling: !memberList.atYBeginning

    readonly property var playlist: detailQuery.playlist || ({})
    property bool draftReady: false
    property string baselineName: ""
    property int baselineMode: 1
    property int baselineIntervalSecs: 0
    property bool baselineSynchronizedSelection: true
    property string draftName: ""
    property int draftMode: 1
    property bool draftSynchronizedSelection: true
    property string draftHours: "0"
    property string draftMinutes: "0"
    property string draftSeconds: "0"
    property double draftRevision: 0
    property var baselineEntryIds: []
    property var draftEntryIds: []
    readonly property int draftIntervalInputSecs: (parseInt(draftHours) || 0) * 3600 + (parseInt(draftMinutes) || 0) * 60 + (parseInt(draftSeconds) || 0)
    readonly property bool intervalDirty: draftIntervalInputSecs !== baselineIntervalSecs
    readonly property int draftIntervalSecs: intervalDirty ? Math.max(10, draftIntervalInputSecs) : baselineIntervalSecs
    readonly property bool dirty: draftReady && (draftName !== baselineName || draftMode !== baselineMode || intervalDirty || draftSynchronizedSelection !== baselineSynchronizedSelection || !sameEntryIds(draftEntryIds, baselineEntryIds))
    readonly property bool canEdit: draftReady && Number(playlist.id || 0) > 0 && detailQuery.status === 2 && !mutationQuery.querying

    function sameEntryIds(left, right) {
        if (left.length !== right.length)
            return false;
        for (let i = 0; i < left.length; ++i) {
            if (String(left[i]) !== String(right[i]))
                return false;
        }
        return true;
    }

    function setDraftInterval(seconds) {
        draftHours = String(Math.floor(seconds / 3600));
        draftMinutes = String(Math.floor((seconds % 3600) / 60));
        draftSeconds = String(seconds % 60);
    }

    function resetDraft() {
        const snapshot = detailQuery.playlist || ({});
        if (Number(snapshot.id || 0) <= 0)
            return;
        draftReady = false;
        baselineName = String(snapshot.name || "");
        baselineMode = Number(snapshot.mode || 1);
        baselineIntervalSecs = Number(snapshot.intervalSecs || 0);
        baselineSynchronizedSelection = snapshot.synchronizedSelection !== false;
        draftName = baselineName;
        draftMode = baselineMode;
        draftSynchronizedSelection = baselineSynchronizedSelection;
        setDraftInterval(baselineIntervalSecs);
        baselineEntryIds = detailQuery.entryIds.slice();
        draftEntryIds = baselineEntryIds.slice();
        draftRevision = detailQuery.revision;
        draftReady = true;
    }

    function acceptDraft() {
        const acceptedIntervalSecs = draftIntervalSecs;
        baselineName = draftName;
        baselineMode = draftMode;
        baselineIntervalSecs = acceptedIntervalSecs;
        baselineSynchronizedSelection = draftSynchronizedSelection;
        setDraftInterval(baselineIntervalSecs);
        baselineEntryIds = draftEntryIds.slice();
    }

    function removeDraftEntry(entryId) {
        const index = draftEntryIds.indexOf(entryId);
        if (index < 0)
            return;
        const next = draftEntryIds.slice();
        next.splice(index, 1);
        draftEntryIds = next;
    }

    function applyDraft() {
        if (!canEdit)
            return;
        if (!dirty)
            return;
        mutationQuery.update(playlistId, draftName, draftMode, draftIntervalSecs, draftSynchronizedSelection, draftEntryIds, draftRevision);
    }

    MD.Action {
        id: resetAction

        text: qsTr("Reset")
        icon.name: MD.Token.icon.restart_alt
        enabled: root.canEdit && root.dirty
        onTriggered: root.resetDraft()
    }

    MD.Action {
        id: applyAction

        text: qsTr("Apply")
        icon.name: MD.Token.icon.check
        enabled: root.canEdit && root.dirty
        busy: mutationQuery.querying
        onTriggered: root.applyDraft()
    }

    actions: [resetAction, applyAction]

    W.PlaylistDetailQuery {
        id: detailQuery

        playlistId: root.playlistId
        forwardError: false
        onPlaylistChanged: {
            if (!root.dirty)
                root.resetDraft();
        }
    }

    W.PlaylistMutationQuery {
        id: mutationQuery

        forwardError: false
        onDone: {
            if (mutationQuery.status === 3) {
                W.Action.toast(mutationQuery.error || qsTr("Playlist update failed"), 6000, 1, null);
                detailQuery.delayReload();
                return;
            }
            root.acceptDraft();
            W.Action.toast(qsTr("Playlist updated"));
            detailQuery.delayReload();
        }
    }

    Connections {
        target: W.Notify
        function onPlaylistChanged() {
            detailQuery.delayReload();
        }
    }

    contentItem: Item {
        implicitWidth: root.implicitWidth
        implicitHeight: root.implicitHeight

        MD.VerticalListView {
            id: memberList

            anchors.fill: parent
            clip: true
            model: detailQuery.data
            spacing: 0
            topMargin: 8
            bottomMargin: 20
            leftMargin: 16
            rightMargin: 16

            header: ColumnLayout {
                width: memberList.contentWidth
                spacing: 12

                MD.TextField {
                    Layout.fillWidth: true
                    mdState.size: MD.Enum.S
                    enabled: root.canEdit
                    placeholderText: qsTr("Name")
                    text: root.draftName
                    onTextEdited: root.draftName = text
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 500 ? 2 : 1
                    columnSpacing: 12
                    rowSpacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 180
                        spacing: 4

                        MD.Text {
                            text: qsTr("Mode")
                            typescale: MD.Token.typescale.label_medium
                            color: MD.Token.color.on_surface_variant
                        }

                        MD.SegmentedButtonGroup {
                            Layout.fillWidth: true
                            size: MD.Enum.S
                            enabled: root.canEdit

                            MD.SegmentedButton {
                                text: qsTr("Sequential")
                                checked: root.draftMode === 1
                                onClicked: root.draftMode = 1
                            }

                            MD.SegmentedButton {
                                text: qsTr("Shuffle")
                                checked: root.draftMode === 2
                                onClicked: root.draftMode = 2
                            }

                            MD.SegmentedButton {
                                text: qsTr("Random")
                                checked: root.draftMode === 3
                                onClicked: root.draftMode = 3
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: intervalRow.implicitWidth
                        spacing: 4

                        MD.Text {
                            text: qsTr("Rotation interval")
                            typescale: MD.Token.typescale.label_medium
                            color: MD.Token.color.on_surface_variant
                        }

                        RowLayout {
                            id: intervalRow

                            Layout.fillWidth: true
                            spacing: 6

                            MD.TextField {
                                id: hoursField

                                Layout.preferredWidth: 48
                                mdState.size: MD.Enum.S
                                enabled: root.canEdit
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 0; top: 999 }
                                text: root.draftHours
                                onTextEdited: root.draftHours = text
                            }

                            MD.Text {
                                text: "h"
                                typescale: MD.Token.typescale.body_small
                                color: MD.Token.color.on_surface_variant
                            }

                            MD.TextField {
                                id: minutesField

                                Layout.preferredWidth: 48
                                mdState.size: MD.Enum.S
                                enabled: root.canEdit
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 0; top: 59 }
                                text: root.draftMinutes
                                onTextEdited: root.draftMinutes = text
                            }

                            MD.Text {
                                text: "m"
                                typescale: MD.Token.typescale.body_small
                                color: MD.Token.color.on_surface_variant
                            }

                            MD.TextField {
                                id: secondsField

                                Layout.preferredWidth: 48
                                mdState.size: MD.Enum.S
                                enabled: root.canEdit
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 0; top: 59 }
                                text: root.draftSeconds
                                onTextEdited: root.draftSeconds = text
                            }

                            MD.Text {
                                text: "s"
                                typescale: MD.Token.typescale.body_small
                                color: MD.Token.color.on_surface_variant
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        MD.Text {
                            text: qsTr("Synchronized selection")
                            typescale: MD.Token.typescale.body_large
                            color: MD.Token.color.on_surface
                        }

                        MD.Text {
                            Layout.fillWidth: true
                            text: root.draftMode === 1
                                ? qsTr("Sequential mode always uses the same wallpaper on every target")
                                : qsTr("Use the same wallpaper on every target; rotation timing stays synchronized when disabled")
                            typescale: MD.Token.typescale.body_small
                            color: MD.Token.color.on_surface_variant
                            wrapMode: Text.WordWrap
                        }
                    }

                    MD.Switch {
                        enabled: root.canEdit && root.draftMode !== 1
                        checked: root.draftMode === 1 || root.draftSynchronizedSelection
                        onClicked: root.draftSynchronizedSelection = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8

                    MD.Text {
                        Layout.fillWidth: true
                        text: qsTr("Wallpapers")
                        typescale: MD.Token.typescale.title_small
                        color: MD.Token.color.on_surface
                    }

                    MD.Text {
                        text: String(root.draftEntryIds.length)
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.on_surface_variant
                    }
                }

                MD.Divider {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 8
                }
            }

            delegate: Item {
                id: wallpaperDelegate
                required property int index
                required property var model

                readonly property string entryId: String(model.id_proto || "")
                readonly property bool retained: root.draftEntryIds.indexOf(entryId) >= 0
                width: memberList.contentWidth
                height: retained ? wallpaperItem.implicitHeight + 6 : 0
                visible: retained

                MD.ListItem {
                    id: wallpaperItem

                    index: wallpaperDelegate.index
                    model: wallpaperDelegate.model
                    width: parent.width
                    height: implicitHeight
                    radius: 10
                    text: model.name || qsTr("Untitled")
                    supportText: String(root.draftEntryIds.indexOf(wallpaperDelegate.entryId) + 1) + " · " + String(model.wpType || "")
                    heightMode: MD.Enum.ListItemTwoLine
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    mdState.backgroundColor: MD.Token.color.surface_container

                    leader: W.ThumbnailImage {
                        implicitWidth: 72
                        implicitHeight: 48
                        source: wallpaperItem.model.preview || ""
                        resource: wallpaperItem.model.resource || ""
                        wpType: wallpaperItem.model.wpType || ""
                        fillMode: Image.PreserveAspectCrop
                    }

                    trailing: MD.IconButton {
                        mdState.size: MD.Enum.XS
                        enabled: root.canEdit
                        icon.name: MD.Token.icon.delete
                        onClicked: root.removeDraftEntry(wallpaperDelegate.entryId)
                        MD.ToolTip.visible: hovered
                        MD.ToolTip.text: qsTr("Remove from playlist")
                    }
                }
            }
        }

        MD.BusyIndicator {
            anchors.centerIn: parent
            running: detailQuery.querying && memberList.count === 0
        }

        MD.Text {
            anchors.centerIn: parent
            width: Math.max(0, parent.width - 48)
            visible: detailQuery.status === 2 && root.draftEntryIds.length === 0
            text: qsTr("No wallpapers in this playlist")
            typescale: MD.Token.typescale.body_large
            color: MD.Token.color.on_surface_variant
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        MD.Text {
            anchors.centerIn: parent
            width: Math.max(0, parent.width - 48)
            visible: detailQuery.status === 3
            text: detailQuery.error || qsTr("Playlist unavailable")
            typescale: MD.Token.typescale.body_medium
            color: MD.Token.color.error
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
