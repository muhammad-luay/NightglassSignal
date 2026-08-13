# UI

The current player-facing surface is `UBMNativeUIWidget`, a runtime-authored
UMG widget created by `ABMHUD`. It supplies the splash/main menu, campaign
selection, Signal Control lobby, settings, controls, How To Play, pause,
mission HUD, scoreboard, death, and results states without requiring binary
Widget Blueprint assets.

`UBMNativeUIWidget` is presentation-only: it reads `ABMHUD`,
`ABMPlayerController`, and `ABMGameState` state and routes button actions back
through the controller. Authored Widget Blueprints can replace the native
surface while preserving those read-only hooks and action contracts.

The legacy `ABMHUD` canvas drawing remains as a compatibility fallback for
headless/editor paths. It is not the primary player-visible UI in the current
staged Mac build.
