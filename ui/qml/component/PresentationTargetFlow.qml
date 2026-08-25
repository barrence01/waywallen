pragma ComponentBehavior: Bound
import QtQuick
import Qcm.Material as MD

Flow {
    id: control

    required property var targetState
    property string allToolTip: ""

    spacing: 6

    MD.FilterChip {
        text: qsTr("All")
        enabled: control.enabled && control.targetState.hasTargets
        checked: control.targetState.allTargets
        onClicked: control.targetState.selectAll()

        MD.ToolTip.visible: hovered && control.allToolTip.length > 0
        MD.ToolTip.text: control.allToolTip
    }

    Repeater {
        model: control.targetState.targets

        MD.FilterChip {
            required property var modelData

            enabled: control.enabled
            width: Math.min(implicitWidth, modelData.maximumWidth)
            text: modelData.label
            icon.name: modelData.iconName
            checked: control.targetState.isSelected(modelData.key)
            onClicked: control.targetState.toggleTarget(modelData.key)

            MD.ToolTip.visible: hovered && modelData.toolTip.length > 0
            MD.ToolTip.text: modelData.toolTip
        }
    }
}
