# Release notes: GP-100 Firmware Builder 0.3.2

Draft text for the GitHub release. 0.3.2 refines the pedal UI of 0.3.1 after testing it on the pedal. The effects and the app are otherwise as in 0.3.1.

## Pedal UI changes

* **Unsaved edits are visible and protected.** The `*` next to the preset number now also shows in Play mode (the mode tag used to cover it).
  Turning PARA in Play with unsaved edits first shows **UNSAVED CHANGES**; turn PARA again to go on (the edits are lost) or press SAVE.
  The footswitches never ask, so changing presets on stage is unchanged. If you do not want the warning, turn it off in
  **GLOBAL > Editing** (Quick 3, "Unsaved"); the setting is saved on the pedal.
* **All effect frames look the same** (grey); only the selected effect has the red frame, and a bypassed effect is darker. Before, a
  yellow, a blue and a red frame could be visible at once.
* **The effect list stays inside its group.** PARA and Q2 wrap from the last effect of a group to its first (7/7 to 1/7); Q1 changes the group.
* **Signal Chain is only for structure.** The "change effect" control is gone (Q2 is empty; PARA on an effect does nothing). Changing an effect is done in Edit (EDIT).
  Q1 is still the footswitch, Q3 still moves the effect, PARA on [+] adds one, and the EDIT menu has Insert, Remove and Assign EXP.
* **The bottom hint line alternates** between two hints every 1.5 seconds on the Edit and Signal Chain screens, so "hold PARA: signal chain" is now visible in Edit.
* The on-pedal guide describes all of this.
* **Clear preset** in the Signal Chain menu (EDIT): removes all effects to start from scratch, after a confirmation. Name, tempo and volume stay, and EXIT undoes it right afterwards.
* **An empty preset** now says "Hold PARA to add effects" in Edit (effects are added in Signal Chain), instead of "Press EDIT to add".
