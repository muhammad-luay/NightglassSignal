# BLACK MERIDIAN

Black Meridian is an original near-future first-person military thriller
vertical slice for Windows PC. The playable slice is intentionally source-first:
its greybox spaces, placeholder weapons, HUD, and lighting are generated at
runtime so combat can be tested before binary art assets are committed.

The project is isolated from the repository's older `MassCrowdSurvival` demo:

```text
NightglassSignal/NightglassSignal.uproject
```

Start with the [game design document](Docs/Game-Design-Document.md), then read
[setup](Docs/Setup.md) and [technical architecture](Docs/Technical-Architecture.md).

## Current vertical slice

- First-person predicted CharacterMovementComponent movement.
- Walk, sprint, tactical sprint, crouch, prone, jump, server-authoritative mantle, slide,
  lean, camera bob, adjustable FOV settings, and controller mappings.
- Serialized data-driven profiles for the ARX-4, Kestrel-9, Rook DMR,
  Breach-12, Solace pistol, Arc-7 launcher, frag, and Veil tactical throwable,
  with runtime fallbacks for partial/cooked-out checkouts. Weapon profiles own
  mesh references and the `Grip`/`Muzzle` socket contract.
- Server-authoritative hitscan/projectile fire, ammo, reload, damage, hit
  zones, falloff, physical-surface rules with a greybox tag fallback,
  replicated health, shot-spread recovery, recoil, procedural first-person
  weapon sway, reload interruption, frag inventory, and Veil suppression.
- Optional authored character animation data and interfaces expose locomotion,
  aim offsets, montage events, separate first-person arms, and third-person
  presentation. Optional weapon audio/Niagara profiles are guarded out of
  dedicated-server execution; the source-only fallback remains available.
- Sight-perception enemy state machine with patrol, investigation, engagement,
  flanking movement, delayed reaction, shared target memory, and aggression
  tuning.
- Friendly squad formation, objective-aware breach/advance/defense movement,
  thin-cover selection, independent visible-enemy engagement, and assist/revive
  behavior.
- Tap/hold interaction actions for doors, ladders, pickups, switches, planting,
  breach, revive, mounted weapons, and mission events, with server-owned
  interaction requests.
- Runtime greybox for Operation Glasswake, mission phases, subtitles,
  checkpoint index, optional intelligence interaction, checkpoint health/
  transform/loadout/throwable-inventory restoration, phase-gated mission
  props, checkpoint world reset, breach barrier destruction, jammer choice,
  profile progression, extraction, and final defense.
- Player-facing onboarding with a plain-language How To Play screen, readable
  phase cards, active objective beacons, and an explicit follow-beacon /
  interact / clear-hostiles / extract loop. The in-game HUD also keeps the
  phase-specific action and mission flow visible. The first-minute briefing
  keeps the immediate destination above the card and the persistent route
  strip spells out `TEAL MOVE / AMBER FIGHT / ORANGE HOLD E / YELLOW EXTRACT`;
  hold interactions show progress with release-to-cancel feedback.
- Signal Control game mode with three rotating nodes, team assignment,
  authoritative team/per-player objective scoring, mode travel, runtime
  lobby/start countdown, result state, bot fill, kill feed, and spawn scoring
  that avoids visible enemies and recent combat locations.
- Development-only UE5.8.1 Mac combat/network probe for a real remote-client
  fire request, server-authoritative ammo/damage, replicated ammo/health, and
  probe-owned disconnect bot takeover.
- Development-only UE5.8.1 Mac interaction probe drives the real relay breach,
  optional archive console, and extraction boat hold path through mission
  completion.
- Twenty selectable runtime campaign operations with level-specific starts,
  route anchors, palette treatments, encounter budgets, role cadence, and
  weapon pickups. Walking within 180 cm of a weapon or ammo pickup
  auto-collects it; weapons grant bonus reserve ammo and equip immediately.
- Per-operation environment dressing: every campaign entry has its own terrain,
  prop set, sky, lighting grade, fog profile, ambient sound bed, and weather
  treatment. The seven environment families cover open/dry, storm/rain,
  snow/ice, desert/dust, forest/mist, underground, and interior spaces;
  rain and snow use both deterministic runtime particles and Niagara accents.
  Campaign play suppresses the saved Glasswake shell and builds a distinct
  collision-bearing structural layout for each operation: rooms, walls,
  barriers, cover, platforms, stairs, route openings, and encounter spaces
  change with the selected level. Only the invisible lower safety floor and
  perimeter recovery colliders are shared; the twenty layouts remain runtime
  generated rather than shipping twenty separate authored `.umap` files.
- Runtime-authored UMG implementations of splash/main menu, campaign
  selection, Signal Control lobby, settings, controls, How To Play, pause, HUD,
  scoreboard, death, results, objective prompts, hit markers, and damage
  direction. Settings navigation persists control/accessibility/audio values;
  authored Widget Blueprints can replace the native surface through the same
  HUD/controller hooks, with the legacy canvas path retained as a fallback.

## Verification boundary

`Scripts/validate_nightglass.py` checks the source contract without Unreal.
The project remains pinned to Unreal Engine 5.1 for its intended Windows
build. The current workstation has Unreal Engine 5.8.1 installed at
`/Users/Shared/Epic Games/UE_5.8`; that newer engine has now passed the Mac
editor build, 34-test automation suite, Mac game-target build, a cooked/staged
client smoke, and a development listen-server probe with two local clients.
Those results are compatibility evidence only: UE5.1, Win64
packaging, dedicated-server packaging, and the packaged/release two-client
target smoke remain explicit gates in [Known Issues](Docs/Known-Issues.md).

To capture a packaged campaign environment on macOS, set `CAMPAIGN_LEVEL` to
`0` through `19`. Use `NO_SOUND=0` when verifying the ambient bed:

```bash
STAGED_APP="$PWD/artifacts/package-environment-themes-20260810/NightglassSignalDemo.app" \
CAMPAIGN_LEVEL=17 NO_SOUND=0 \
./Scripts/run_packaged_combat_showcase_capture.sh
```
