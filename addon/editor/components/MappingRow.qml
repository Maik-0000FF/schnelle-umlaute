import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    radius: Theme.radiusMd
    readonly property var view: ListView.view
    // Exactly one highlight at a time, following the list's active input mode
    // (view.keyboardActive): the keyboard-current row while navigating by
    // keyboard, otherwise the mouse-hovered row. Both use the same surfaceHover
    // tone, so switching input never shows two competing highlights.
    readonly property bool highlighted:
        view && (view.keyboardActive
                 ? (ListView.isCurrentItem && view.activeFocus)
                 : hoverHandler.hovered)
    // Number of this row's drop areas currently under a foreign-chip drag. A
    // single row-level DropArea can't see the whole row once the per-chip drop
    // areas sit on top of it, so each area bumps this counter on a foreign
    // enter/exit and the row lights up while any of them is hovered (over a
    // chip OR the empty space).
    property int foreignDragCount: 0
    // Gated on an active chip drag (view.chipDragging) and reset when the drag
    // ends (see the Connections below), so a leaked count, from a drop that
    // leaves a DropArea's containsDrag stuck, can never keep the border lit.
    readonly property bool dropTarget:
        foreignDragCount > 0 && view && view.chipDragging
    color: highlighted ? Theme.surfaceHover : "transparent"
    border.color: dropTarget ? Theme.accent
                             : (editing ? Theme.borderFocus : "transparent")
    border.width: 1
    height: col.implicitHeight + 8

    // HoverHandler is a passive grabber: unlike a MouseArea with hoverEnabled,
    // it stays "hovered" while the cursor is over child pointer handlers
    // (drag handle, buttons) — which otherwise stole hover and made the row
    // background flicker.
    // Only reports which row the pointer is over (for the mouse-mode
    // highlight). The mode switch itself is driven by genuine pointer movement
    // on the non-scrolling list card (see Mappings.qml), not by hover changes,
    // which also fire when rows scroll under a still cursor.
    HoverHandler { id: hoverHandler }

    // Reset the drop-target counter when a chip drag ends, so a stuck
    // containsDrag can never leave this row highlighted after the drag.
    Connections {
        target: root.view
        function onChipDraggingChanged() {
            if (root.view && !root.view.chipDragging)
                root.foreignDragCount = 0;
        }
    }

    // Row-level drop target for a cross-row variant move: catches a chip
    // dragged onto this row's empty area (drops onto a chip go through the
    // per-chip DropArea). A drop from another mapping moves the variant here.
    DropArea {
        id: rowDrop
        anchors.fill: parent
        // A foreign-chip drag over the row's empty space, derived from the
        // reliable containsDrag property (not enter/exit events, which can leak
        // and leave the highlight stuck), so foreignDragCount stays balanced.
        readonly property bool foreignHover:
            containsDrag && drag.source
            && drag.source.sourceInput !== undefined
            && drag.source.sourceInput !== root.inputText
        onForeignHoverChanged: root.foreignDragCount += foreignHover ? 1 : -1
        onDropped: (drop) => {
            if (!drop.source || !root.modelRef)
                return;
            // A chip from another mapping: move the variant onto this row.
            if (drop.source.sourceInput !== root.inputText) {
                root.modelRef.moveVariant(drop.source.sourceInput,
                    drop.source.variant, root.inputText);
                return;
            }
            // Same row, dropped in the free area (not on a chip): send this
            // variant to the end, so a drop beside the chips reorders too.
            let order = root.variantList.slice();
            const from = order.indexOf(drop.source.variant);
            if (from < 0)
                return;
            order.splice(from, 1);
            order.push(drop.source.variant);
            root.modelRef.setVariantOrder(root.inputText, order);
        }
    }

    // Clicking anywhere on the row (outside the action buttons / drag handle)
    // makes it the current row and moves keyboard focus to the list, so arrow
    // keys continue from where the mouse landed. Declared before the content so
    // the buttons and drag handle on top still receive their own clicks.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: {
            const view = root.ListView.view;
            if (!view)
                return;
            // Any open edit in the list (this row or another) owns focus; a
            // background click must not pull it away and strand the edit. The
            // ListView holds the single list-wide editingIndex.
            if (view.editingIndex !== -1)
                return;
            // A click is mouse input: keep the hover look on this row rather
            // than the keyboard highlight, even though we also set current+focus
            // so arrow keys can continue from here.
            view.keyboardActive = false;
            view.currentIndex = root.rowIndex;
            view.forceActiveFocus();
        }
    }

    // No border animation: the cross-row drop-target highlight must snap on and
    // off instantly while dragging, an animated fade reads as laggy.
    property int rowIndex: -1
    property string inputText: ""
    property string outputText: ""
    // The output split into its cycling variants, matching the engine's
    // splitOutputs: ",," is an escaped literal comma, a single "," separates
    // variants, empty segments are dropped. Done with this escaping-aware pass
    // (not a naive split(",")) so a variant containing a comma stays one chip.
    readonly property var variantList: {
        let out = [];
        let cur = "";
        const s = root.outputText;
        for (let i = 0; i < s.length; ++i) {
            if (s[i] === ",") {
                if (i + 1 < s.length && s[i + 1] === ",") {
                    cur += ","; ++i;
                } else if (cur.length > 0) {
                    out.push(cur); cur = "";
                }
            } else {
                cur += s[i];
            }
        }
        if (cur.length > 0) out.push(cur);
        return out;
    }
    property var modelRef: null
    property var settingsModel: null
    property bool editing: false

    // Read-only input cell width, shared with the error/warning rows below so
    // their text lines up under the output column.
    readonly property int inputCellWidth: 44

    signal removeRequested()
    signal editStartRequested()
    signal editEndRequested()

    onEditingChanged: {
        if (editing) {
            inputEdit.text = inputText;
            outputEdit.text = outputText;
            outputEdit.forceActiveFocus();
        }
    }

    // isActiveLeaderKey / inputErrorFor are method calls — QML can't track the
    // state behind them, so bump a tick when leaders change and reference it
    // in the conflict binding to force re-evaluation.
    property int leadersTick: 0
    Connections {
        target: root.settingsModel
        function onLeadersChanged() { root.leadersTick++; }
    }

    readonly property string editInputError:
        modelRef && editing
            ? modelRef.inputErrorFor(inputEdit.text, rowIndex)
            : ""
    readonly property string editOutputError:
        modelRef && editing ? modelRef.outputErrorFor(outputEdit.text) : ""
    readonly property bool editLeaderConflict: {
        leadersTick; // establish dependency
        return editing && inputEdit.text.length > 0 && settingsModel &&
            settingsModel.isActiveLeaderKey(inputEdit.text);
    }
    readonly property bool editOutputInvalid:
        editing && (outputEdit.text.length === 0 ||
                    !modelRef.validateOutput(outputEdit.text))
    readonly property bool editValid:
        editing && inputEdit.text.length > 0 && editInputError === "" &&
        !editOutputInvalid

    // Read-only view uses inputText directly so the row still flags dead
    // mappings when it isn't being edited.
    readonly property bool staticLeaderConflict: {
        leadersTick; // establish dependency
        return !editing && inputText.length > 0 && settingsModel &&
            settingsModel.isActiveLeaderKey(inputText);
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingSm
        spacing: Theme.spacingXxs

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

        Item {
            width: 16
            height: Theme.controlHeight
            visible: !root.editing

            Text {
                anchors.centerIn: parent
                text: "⠿"
                color: dragArea.containsMouse || dragArea.pressed
                    ? Theme.text : Theme.textMuted
                font.pixelSize: Theme.fontIcon
                Behavior on color { ColorAnimation { duration: Theme.animShort } }
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor
                // Keep the grab once the row drag starts, so a scrollable list
                // (Flickable) doesn't steal the vertical gesture and pan instead.
                preventStealing: true

                property real pressY: 0
                property int originalIndex: -1

                onPressed: (mouse) => {
                    const view = root.ListView.view;
                    pressY = mapToItem(view, mouse.x, mouse.y).y;
                    originalIndex = root.rowIndex;
                }

                onPositionChanged: (mouse) => {
                    if (!pressed || originalIndex < 0 || !root.modelRef) return;
                    const view = root.ListView.view;
                    const currentY = mapToItem(view, mouse.x, mouse.y).y;
                    const rowPitch = root.height + (view.spacing || 0);
                    const delta = Math.round((currentY - pressY) / rowPitch);
                    const targetIndex = Math.max(0, Math.min(
                        root.modelRef.count - 1,
                        originalIndex + delta
                    ));
                    if (targetIndex !== root.rowIndex) {
                        root.modelRef.moveMapping(root.rowIndex, targetIndex);
                    }
                }

                onReleased: {
                    originalIndex = -1;
                }
            }

            ThemedToolTip {
                hovered: dragArea.containsMouse
                text: qsTr("Drag to reorder")
            }
        }

        Rectangle {
            width: root.inputCellWidth
            height: Theme.controlHeight
            radius: Theme.radiusSm
            color: Theme.background
            border.color: root.staticLeaderConflict ? Theme.warning : Theme.border
            border.width: 1
            visible: !root.editing
            Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

            Text {
                anchors.centerIn: parent
                text: root.inputText
                color: Theme.text
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontStrong
            }
        }

        ThemedTextField {
            id: inputEdit
            visible: root.editing
            Layout.preferredWidth: 80
            text: root.inputText
            maximumLength: 4
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontStrong
            horizontalAlignment: TextInput.AlignHCenter
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: root.editInputError !== ""
                    ? Theme.error
                    : (root.editLeaderConflict ? Theme.warning : Theme.accent)
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: Theme.animShort } }
            }
            Keys.onEscapePressed: cancelEdit()
            // Handle Escape locally (cancel the edit) instead of letting the
            // window-level Esc shortcut close the whole editor.
            Keys.onShortcutOverride: (event) => {
                if (event.key === Qt.Key_Escape)
                    event.accepted = true;
            }
        }

        Text {
            text: "→"
            color: Theme.textMuted
            font.pixelSize: Theme.fontIcon
        }

        // Output variants as chips: click a chip's ✕ to drop that variant, or
        // drag a chip to reorder within the row. Editing the whole comma string
        // still happens via the pencil (outputEdit below). The variant list is
        // pre-split by the model (VariantsRole), so a variant containing a
        // literal comma is one chip and never mis-splits.
        Flow {
            id: chipFlow
            Layout.fillWidth: true
            visible: !root.editing
            spacing: Theme.spacingXs
            move: Transition {
                NumberAnimation { properties: "x,y"; duration: Theme.animShort }
            }
            Repeater {
                model: root.variantList
                delegate: DropArea {
                    id: chipDrop
                    required property string modelData
                    required property int index
                    implicitWidth: chip.implicitWidth
                    implicitHeight: chip.implicitHeight
                    // A foreign-chip drag over this chip, derived from the
                    // reliable containsDrag property so foreignDragCount can't
                    // get stuck (see the row-level foreignHover).
                    readonly property bool foreignHover:
                        containsDrag && drag.source
                        && drag.source.sourceInput !== undefined
                        && drag.source.sourceInput !== root.inputText
                    onForeignHoverChanged:
                        root.foreignDragCount += foreignHover ? 1 : -1
                    // Move the dragged variant to this chip's ORIGINAL slot.
                    // Using the target's original index (not indexOf after the
                    // splice) makes the move direction-aware: a left chip dropped
                    // on a right chip lands after it, and a right chip dropped on
                    // a left chip lands before it. Dropping on its own chip
                    // (from === index) reinserts in place, a no-op.
                    onDropped: (drop) => {
                        if (!drop.source || !root.modelRef)
                            return;
                        // A chip from another mapping: move the variant onto
                        // this row's input instead of reordering.
                        if (drop.source.sourceInput !== root.inputText) {
                            root.modelRef.moveVariant(drop.source.sourceInput,
                                drop.source.variant, root.inputText);
                            return;
                        }
                        let order = root.variantList.slice();
                        const from = order.indexOf(drop.source.variant);
                        if (from < 0)
                            return;
                        order.splice(from, 1);
                        order.splice(chipDrop.index, 0, drop.source.variant);
                        root.modelRef.setVariantOrder(root.inputText, order);
                    }
                    Rectangle {
                        id: chip
                        property string variant: chipDrop.modelData
                        // Which mapping this chip belongs to, so a drop target
                        // can tell a same-row reorder from a cross-row move.
                        property string sourceInput: root.inputText
                        width: implicitWidth
                        height: implicitHeight
                        implicitWidth: chipRow.implicitWidth
                                       + 2 * Theme.chipPaddingH
                        implicitHeight: Theme.controlHeight
                        radius: Theme.radiusSm
                        color: chip.Drag.active ? Theme.surfaceHover
                                                : Theme.background
                        border.color: (dragMouse.containsMouse || chip.Drag.active)
                                      ? Theme.accent : Theme.border
                        border.width: 1
                        // Float above every row (reparented to the list) while
                        // dragging, so a cross-row drag stays visible.
                        z: chip.Drag.active ? 100 : 0
                        Behavior on border.color {
                            ColorAnimation { duration: Theme.animShort }
                        }
                        Drag.active: dragMouse.drag.active
                        Drag.hotSpot.x: width / 2
                        Drag.hotSpot.y: height / 2
                        // Report the drag state up to the list so every row can
                        // drop its highlight the moment this drag ends.
                        readonly property bool dragging: chip.Drag.active
                        onDraggingChanged: if (root.view)
                            root.view.chipDragging = dragging
                        states: State {
                            when: chip.Drag.active
                            // ParentChange keeps the on-screen position, so the
                            // reparent to the list is jump-free.
                            ParentChange { target: chip; parent: root.view }
                        }

                        // Drag body: the move cursor and hover affordance live
                        // here (hoverEnabled so both react on hover). Declared
                        // before the ✕ so the ✕ still gets its own clicks.
                        MouseArea {
                            id: dragMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            drag.target: chip
                            cursorShape: Qt.SizeAllCursor
                            // Keep the grab so a scrollable list doesn't steal
                            // the chip drag and pan the page instead.
                            preventStealing: true
                            onReleased: {
                                // Clear the drag flag here (reliable, the chip is
                                // still alive) rather than via the chip's dragging
                                // binding, which is missed when the drop rebuilds
                                // the row and destroys the chip first.
                                if (root.view)
                                    root.view.chipDragging = false;
                                chip.Drag.drop();
                            }
                        }

                        ThemedToolTip {
                            hovered: dragMouse.containsMouse
                                     && !chipX.containsMouse && !chip.Drag.active
                            text: qsTr("Drag to reorder")
                        }

                        RowLayout {
                            id: chipRow
                            anchors.centerIn: parent
                            spacing: Theme.spacingSm
                            Text {
                                text: chip.variant
                                color: Theme.text
                                font.family: Theme.fontFamilyMono
                                font.pixelSize: Theme.chipFont
                            }
                            // Circular ✕ close button, the conventional chip
                            // delete affordance: a muted circle that separates
                            // the remove control from the variant text and turns
                            // red on hover.
                            Rectangle {
                                id: chipClose
                                // Hidden on a single-chip row: the sole output
                                // can't be removed here (a mapping needs one),
                                // delete the whole mapping via the trash button.
                                visible: root.variantList.length > 1
                                implicitWidth: Theme.chipFont + Theme.spacingXs
                                implicitHeight: implicitWidth
                                radius: implicitHeight / 2
                                color: chipX.containsMouse ? Theme.error
                                                           : Theme.surfaceHover
                                Behavior on color {
                                    ColorAnimation { duration: Theme.animShort }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: Theme.iconClear
                                    color: chipX.containsMouse ? Theme.accentText
                                                               : Theme.textMuted
                                    font.pixelSize: Theme.chipFont - 3
                                }
                                MouseArea {
                                    id: chipX
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (root.modelRef)
                                        root.modelRef.removeVariant(
                                            root.inputText, chip.variant);
                                    ThemedToolTip {
                                        hovered: chipX.containsMouse
                                        text: qsTr("Remove")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ThemedTextField {
            id: outputEdit
            visible: root.editing
            Layout.fillWidth: true
            text: root.outputText
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontStrong
            horizontalAlignment: TextInput.AlignLeft
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: root.editOutputInvalid && text.length > 0
                    ? Theme.error : Theme.accent
                border.width: 1
            }
            onAccepted: if (root.editValid) confirmEdit()
            Keys.onEscapePressed: cancelEdit()
            // Handle Escape locally (cancel the edit) instead of letting the
            // window-level Esc shortcut close the whole editor.
            Keys.onShortcutOverride: (event) => {
                if (event.key === Qt.Key_Escape)
                    event.accepted = true;
            }
        }

        ToolButton {
            id: applyBtn
            // Mouse affordance only: keyboard uses the roving list (Enter/F2),
            // and grabbing focus on click would let Space re-fire the button.
            focusPolicy: Qt.NoFocus
            text: root.editing ? Theme.iconCheck : Theme.iconEdit
            enabled: !root.editing || root.editValid
            ThemedToolTip {
                hovered: applyBtn.hovered
                text: root.editing ? qsTr("Apply") : qsTr("Edit")
            }
            contentItem: Text {
                text: applyBtn.text
                color: !applyBtn.enabled
                    ? Theme.border
                    : (applyBtn.hovered ? Theme.brand : Theme.textMuted)
                font.pixelSize: Theme.fontIcon
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: "transparent" }
            onClicked: {
                if (root.editing) confirmEdit();
                else startEdit();
            }
        }

        ToolButton {
            id: deleteBtn
            // Mouse affordance only: keyboard uses the roving list (Delete), and
            // grabbing focus on click would let Space re-open the delete dialog.
            focusPolicy: Qt.NoFocus
            text: root.editing ? Theme.iconCancel : Theme.iconTrash
            ThemedToolTip {
                hovered: deleteBtn.hovered
                text: root.editing ? qsTr("Cancel") : qsTr("Delete")
            }
            contentItem: Text {
                text: parent.text
                color: parent.hovered ? Theme.error : Theme.textMuted
                font.pixelSize: Theme.fontIcon
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: "transparent" }
            onClicked: {
                if (root.editing) cancelEdit();
                else root.removeRequested();
            }
        }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: root.inputCellWidth + Theme.spacingMd
            visible: root.editing && root.editInputError !== "" &&
                     inputEdit.text.length > 0
            text: root.editInputError
            color: Theme.error
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        // Output error (e.g. a lone "," with no variants), shown only when the
        // input is otherwise fine so the two error lines don't stack.
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: root.inputCellWidth + Theme.spacingMd
            visible: root.editing && root.editOutputError !== "" &&
                     outputEdit.text.length > 0 && root.editInputError === ""
            text: root.editOutputError
            color: Theme.error
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: root.inputCellWidth + Theme.spacingMd
            visible: root.staticLeaderConflict ||
                     (root.editing && root.editLeaderConflict &&
                      root.editInputError === "")
            text: qsTr("This key is configured as a Leader: mapping will not work")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Delete && !root.editing) {
            root.removeRequested();
            event.accepted = true;
        }
    }

    function startEdit() { root.editStartRequested(); }

    function confirmEdit() {
        if (root.modelRef && root.modelRef.updateMapping(
                root.rowIndex, inputEdit.text, outputEdit.text)) {
            root.editEndRequested();
        }
    }

    function cancelEdit() { root.editEndRequested(); }
}
