pragma ComponentBehavior: Bound
pragma ValueTypeBehavior: Assertable
import QtQuick
import Qcm.Material as MD
import waywallen.ui as W

Item {
    id: root

    required property var model
    required property int index
    property var wallpaper: model
    property bool selected: false
    property real itemWidth: width
    property real itemHeight: height

    width: GridView.view ? GridView.view.cellWidth : 0
    height: GridView.view ? GridView.view.cellHeight : 0

    focusPolicy: Qt.StrongFocus

    signal clicked(int modifiers)
    signal selectionRequested(int modifiers)

    readonly property real _preloadMargin: 400
    readonly property int _unloadDelayMs: 500

    readonly property var _view: GridView.view

    readonly property bool _viewBusy:
        !!root._view && (root._view.flicking || root._view.moving)

    readonly property int _rawZone: {
        const view = root._view
        if (!view)
            return 2

        const margin = root._preloadMargin
        const top = view.contentY
        const bottom = top + view.height
        const itemBottom = y + height

        if (itemBottom <= top - margin || y >= bottom + margin)
            return 0

        if (itemBottom <= top || y >= bottom)
            return 1

        return 2
    }

    property bool _imageWanted: true

    function _syncImageWanted() {
        switch (root._rawZone) {
        case 2:
            unloadTimer.stop()
            root._imageWanted = true
            break

        case 1:
            unloadTimer.stop()
            if (!root._viewBusy)
                root._imageWanted = true
            break

        case 0:
            if (!unloadTimer.running)
                unloadTimer.restart()
            break
        }
    }

    on_RawZoneChanged: _syncImageWanted()
    on_ViewBusyChanged: _syncImageWanted()

    Timer {
        id: unloadTimer
        interval: root._unloadDelayMs
        onTriggered: {
            if (root._rawZone === 0)
                root._imageWanted = false
        }
    }

    Component.onCompleted: _syncImageWanted()

    readonly property int _baseRadius: MD.Token.shape.corner.extra_small
    readonly property int _selectedRadius: MD.Token.shape.corner.large
    readonly property int _radius: root.selected ? root._selectedRadius : root._baseRadius
    readonly property real _selectedInset: root._selectedRadius / 2
    readonly property real cardWidth: Math.min(root.itemWidth, root.width)
    readonly property real cardHeight: Math.min(root.itemHeight, root.height)

    Rectangle {
        anchors.fill: parent
        visible: root.selected
        color: MD.Token.color.primary_container
    }

    Item {
        id: m_card
        width: root.cardWidth
        height: root.cardHeight
        anchors.centerIn: parent

        Item {
            id: m_cell
            anchors.fill: parent
            anchors.margins: 6 + (root.selected ? root._selectedInset : 0)

            Loader {
                id: m_thumb
                anchors.fill: parent
                active: root._imageWanted
                sourceComponent: Component {
                    W.ThumbnailImage {
                        anchors.fill: parent
                        source  : root.wallpaper?.preview ?? ""
                        resource: root.wallpaper?.resource ?? ""
                        wpType  : root.wallpaper?.wpType ?? ""
                        fillMode: Image.PreserveAspectCrop
                        radius: root._radius
                    }
                }
            }

            // Scrim aligns to the image control's bounds; spans the
            // title-top → image-bottom overlap.
            Rectangle {
                anchors.left  : m_thumb.left
                anchors.right : m_thumb.right
                anchors.bottom: m_thumb.bottom
                height: Math.max(0, m_thumb.height - m_title.y)
                visible: height > 0
                radius: root._radius
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.6) }
                }
            }

            MD.Text {
                id: m_title
                anchors.left  : parent.left
                anchors.right : parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                text: root.wallpaper?.name || qsTr("Untitled")
                typescale: MD.Token.typescale.title_small
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 2
                leftPadding: 8
                rightPadding: 8
            }

            MouseArea {
                property bool selectionRequestedByHold: false

                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                onPressed: selectionRequestedByHold = false
                onCanceled: selectionRequestedByHold = false
                onPressAndHold: mouse => {
                    if (mouse.button !== Qt.LeftButton)
                        return;
                    selectionRequestedByHold = true;
                    root.selectionRequested(mouse.modifiers);
                }
                onClicked: mouse => {
                    if (selectionRequestedByHold) {
                        selectionRequestedByHold = false;
                        return;
                    }
                    if (mouse.button === Qt.RightButton) {
                        root.selectionRequested(mouse.modifiers);
                        return;
                    }
                    root.clicked(mouse.modifiers);
                }
            }
        }
    }

    Rectangle {
        anchors.top: m_card.top
        anchors.left: m_card.left
        anchors.margins: 8
        width: 32
        height: 32
        radius: width / 2
        visible: root.selected
        color: MD.Token.color.primary
        border.color: MD.Token.color.primary_container
        border.width: 3

        MD.Icon {
            anchors.centerIn: parent
            name: MD.Token.icon.check
            size: 20
            color: MD.Token.color.on_primary
        }
    }
}
