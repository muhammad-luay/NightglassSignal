# Controls guide

## First mission minute

Press `Enter` on the main menu and again on the campaign card to deploy. The
opening card gives the whole route, while the persistent mission strip repeats
the action key: `TEAL = MOVE`, `AMBER = FIGHT`, `ORANGE = HOLD E`, and
`YELLOW = EXTRACT`. In the mission, use WASD and mouse look to follow the teal
beacon. When the amber beacon appears, stop in that area: red Vanta hostiles
attack there. Aim with RMB, fire with LMB, and reload with R. After the
checkpoint is clear, follow the orange relay-house beacon. Hold `E` at the
orange door, the optional teal console, and the yellow extraction boat.

When contact starts, red Vanta silhouettes (and the red `VANTA` HUD markers)
are the targets. Stay behind cover, aim with RMB, fire with LMB, and reload
with R. If no target is on screen, follow the red `CONTACT // TURN ...` cue.

| Action | Keyboard/mouse | Controller |
| --- | --- | --- |
| Move / look | WASD / mouse; Left/Right also turn | Left / right sticks |
| Fire / aim | LMB / RMB | RT / LT |
| Sprint / tactical sprint | Shift / Caps Lock | Left stick click / reserved |
| Crouch / prone | C / Z | Face Right / reserved |
| Jump / mantle | Space | Face Top |
| Lean / reset | Q / Right Alt / Left Alt | reserved for remapping |
| Reload / weapon switch | R / 1 or 2 | Face Left / D-pad Down |
| Melee / frag / tactical | V / G / X | Right stick click / D-pad Right / D-pad Left |
| Interact | E | Face Bottom |
| Pause / settings / controls / help | Esc / F1 / K (or Fn+F2) / H | Menu button / D-pad selection |
| Front-end menu selection | Enter / arrow keys | D-pad; A confirms; B backs out |
| Settings / controls navigation | Up/Down, Left/Right | D-pad / face buttons where mapped |
| Scoreboard / restart checkpoint | Tab / Backspace | reserved for remapping |

Press `H` at the main menu, deploy screen, pause screen, or during the mission
to reopen the plain-language briefing.

## Mac Pro development playtest bridge

For a remote development package, launch the game on the Mac Pro with
`-BM_RemoteInputPort=1986`. The listener binds only to the Mac Pro's loopback
interface. From this workstation, the repository helper carries commands over
the existing SSH/Tailscale connection:

```text
Scripts/remote_input_client.sh pro 1986 STATE
Scripts/remote_input_client.sh pro 1986 "KEY W DOWN" "KEY W UP" STATE
```

With no command arguments the helper stays interactive. Supported requests are
`STATE`, `PING`, `HELP`, `LOOK <yaw-delta> <pitch-delta>`, and
`KEY <FKey-name> <DOWN|UP|REPEAT>`. Common names include `W`, `A`, `S`, `D`,
`LeftMouseButton`, `RightMouseButton`, `R`, `E`, `SpaceBar`, `LeftShift`, and
`Escape`. This drives the controller's normal input path for development QA;
it is disabled in shipping builds and is not an OS-level keyboard or screen
capture workaround.

`1` and `2` are both weapon-switch aliases during gameplay. `F2` or `K` opens
the Controls screen; it is intentionally separate so pressing `2` cannot pause
the fight or block grenade input. On macOS keyboards whose function row is set
to media keys, use `Fn+F2` or the `K` fallback.

Frag and Veil counts are shown as current/max in the lower-right HUD. The
orange ordnance cache on the approach to the second combat pocket auto-restocks
one Frag and one Veil when either slot has room.

The arena perimeter is solid and cannot be jumped over. Walking into an outer
edge stops the player on the playable floor; leaving the map is not a failure
condition.

If your health reaches `0`, the mission pauses on `SQUAD LOST`; that is the
death screen, not a broken look control. Press `Enter` to restart from the
latest checkpoint or `Esc` to return to the campaign menu. Turning resumes
immediately after the checkpoint reloads.

During the mission, the HUD keeps the current action visible: follow a beacon,
hold the amber checkpoint, clear the named hostile wave, hold `E` at the next
interaction, or defend the pier. Combat areas explicitly say to stay there;
they are no longer presented as navigation waypoints. Holding `E` shows a
progress bar and releasing it cancels the interaction. The mission-flow strip
keeps the complete route visible: `MOVE`, `CLEAR`, `BREACH`, `EXTRACT`, then
`DEFEND`.

FOV, horizontal/vertical sensitivity, hip-fire and ADS sensitivity, subtitle
size, color-blind preset, reduced camera motion, motion blur, audio mix, and
gore reduction are persisted through `UBMGameUserSettings`. The compact
runtime-authored UMG controls screen lets the player select a keyboard/mouse action and cycle its
persisted binding between safe presets. A preset cannot steal a reserved menu
key; when two remappable actions collide, their keyboard keys swap atomically.
Controller mappings remain available and are never changed by a keyboard
remap. Audio routing to authored Sound Classes and arbitrary-key capture remain
release work. The input names are centralized in `Config/DefaultInput.ini`;
front-end menu buttons are selected with the D-pad, confirmed with A, and
backed out with B. Unmapped controller actions are deliberately marked
reserved instead of being claimed as supported.
