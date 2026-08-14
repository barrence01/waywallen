pragma ComponentBehavior: Bound
import QtQuick
import Qcm.Material as MD

Item {
    id: root

    required property int index
    required property string itemId
    required property string title
    required property string previewUrl
    required property string author
    required property int acquisitionState
    required property int remoteCapability
    property real itemWidth: width
    property real itemHeight: height

    signal clicked()

    width: GridView.view ? GridView.view.cellWidth : 0
    height: GridView.view ? GridView.view.cellHeight : 0

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

    readonly property int _radius: MD.Token.shape.corner.extra_small
    readonly property real cardWidth: Math.min(root.itemWidth, root.width)
    readonly property real cardHeight: Math.min(root.itemHeight, root.height)

    Item {
        id: m_card
        width: root.cardWidth
        height: root.cardHeight
        anchors.centerIn: parent

        Item {
            id: m_cell
            anchors.fill: parent
            anchors.margins: 6
            clip: true

            Loader {
                anchors.fill: parent
                active: root._imageWanted
                sourceComponent: Component {
                    AnimatedImage {
                        id: m_thumb
                        anchors.fill: parent
                        source: root.previewUrl
                        fillMode: Image.PreserveAspectCrop
                        horizontalAlignment: Image.AlignHCenter
                        verticalAlignment: Image.AlignVCenter
                        smooth: true
                        cache: true
                        asynchronous: true
                        playing: root._rawZone === 2
                        sourceSize: Qt.size(Math.ceil(width), Math.ceil(height))
                        
                        layer.enabled: root._rawZone === 2
                        layer.effect: MD.RoundClip {
                            corners: MD.Util.corners(root._radius)
                            size: Qt.vector2d(m_thumb.width, m_thumb.height)
                        }
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.top: m_title.top
                anchors.topMargin: -12 // Margem para suavizar a transição acima do texto
                radius: root._radius
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.65) }
                }
            }

            MD.Text {
                id: m_title
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                text: root.title.length > 0 ? root.title : qsTr("Untitled")
                typescale: MD.Token.typescale.title_small
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 2
                leftPadding: 8
                rightPadding: 8
            }

            Rectangle {
                visible: (root.remoteCapability === 1 && root.acquisitionState === 3)
                    || (root.remoteCapability === 2 && root.acquisitionState === 2)
                anchors { top: parent.top; right: parent.right; margins: 6 }
                width: m_badge.implicitWidth + 12
                height: m_badge.implicitHeight + 6
                radius: height / 2
                color: MD.Token.color.primary

                MD.Label {
                    id: m_badge
                    anchors.centerIn: parent
                    text: root.remoteCapability === 2 ? qsTr("Subscribed") : qsTr("Downloaded")
                    typescale: MD.Token.typescale.label_small
                    color: MD.Token.color.on_primary
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.clicked()
            }
        }
    }
}
