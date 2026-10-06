pragma ComponentBehavior: Bound
import Qcm.Material as MD
import waywallen.control as WC

MD.SegmentedButtonGroup {
    id: control

    property int value: WC.Flip.FLIP_NONE
    signal selected(int value)

    size: MD.Enum.XS

    MD.SegmentedButton {
        text: qsTr("None")
        checked: !control.value || control.value === WC.Flip.FLIP_NONE
        onClicked: control.selected(WC.Flip.FLIP_NONE)
    }
    MD.SegmentedButton {
        text: qsTr("Horizontal")
        checked: control.value === WC.Flip.FLIP_HORIZONTAL
        onClicked: control.selected(WC.Flip.FLIP_HORIZONTAL)
    }
    MD.SegmentedButton {
        text: qsTr("Vertical")
        checked: control.value === WC.Flip.FLIP_VERTICAL
        onClicked: control.selected(WC.Flip.FLIP_VERTICAL)
    }
    MD.SegmentedButton {
        text: qsTr("Both")
        checked: control.value === WC.Flip.FLIP_BOTH
        onClicked: control.selected(WC.Flip.FLIP_BOTH)
    }
}
