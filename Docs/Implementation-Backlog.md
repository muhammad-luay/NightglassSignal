# Implementation backlog

## Phase 1 — discovery and architecture

- [x] Inspect repository and identify the existing project as a separate
  top-down UE5.1 demo.
- [x] Isolate the Black Meridian UE5.1 project/plugin boundary.
- [x] Define C++/Blueprint/content ownership and risk gates.
- [x] Define the source-only first playable milestone.

## Phase 2 — core prototype

- [x] Runtime-generated first-person pawn, camera, movement, crouch/prone,
  server-authoritative mantle, slide, lean, and placeholder weapon mesh.
- [x] Bounded discrete jump/mantle input: release clears the inherited jump
  state, airborne/flying and runtime-floor-wait states are rejected, gravity
  is restored before launch, and the current packaged Floodline smoke verifies
  a single `420 cm/s` jump returns to ground without the previous vertical
  runaway.
- [x] Data-driven ARX-4 profile, hitscan, recoil, ammo, reload, falloff,
  hit-zone multipliers, and health.
- [x] Accuracy shot penalty/recovery, data-driven procedural weapon sway, and
  launcher projectile support with server-side detonation.
- [x] Development-only UE5.8.1 Mac combat-feel probe drives the ordinary player
  fire path and records fire-interval rejection, accuracy recovery, and actual
  first-person weapon-root sway; runner:
  `Scripts/run_combat_feel_probe.sh`, evidence:
  `artifacts/combat-feel-probe-local/` and
  `artifacts/combat-feel-probe-packaged-local/`.
- [x] Server-authoritative frag inventory/fuse path and Veil tactical
  suppression effect.
- [x] Runtime greybox cover/floor/walls and minimal objective HUD.
- [x] Perception-driven Vanta enemy and first mission wave.
- [x] Twenty selectable runtime campaign operations with level-specific starts,
  route anchors, palette treatments, pickup weapons, encounter role cadence,
  and increasing encounter budgets; the operations are generated from the
  Entry map rather than authored as separate `.umap` files.
- [x] Profile persistence records which campaign variants have been completed,
  in addition to the total completion count and checkpoint campaign level.
- [x] Proximity weapon/ammo collection within 180 cm, including weapon grant,
  bonus reserve ammo, immediate equip, acquisition feedback, and pickup
  destruction. Each of the twenty campaign operations exposes a defined
  pickup firearm and ammo bundle.
- [x] Non-failing playable bounds: four invisible perimeter walls stop normal
  traversal, and a last-safe-ground recovery restores the existing pawn if a
  physics/cook edge case still drops it below the floor. The packaged Floodline
  boundary-guard probe records `X=-2065.943`, `grounded=yes`, and
  `eliminated=no` at the wall with no `SQUAD LOST` state; the repeatable probe
  is `Scripts/run_boundary_guard_probe.sh` and its current evidence is
  `artifacts/boundary-guard-packaged-20260803/`.
- [ ] Provisioned UE5.1 compile and FPM-01 runtime acceptance.

## Phase 3 — combat expansion

- [x] Extensible profile seams for five weapon roles and two throwables.
- [x] Serialized weapon and throwable data assets generated and validated by
  editor automation, including project mesh/socket references and cooked-path
  guards.
- [x] Attachment-slot data contract.
- [x] Simplified ThinCover penetration path and damage falloff.
- [x] Directional damage, hit-marker, suppression-state seams.
- [x] Runtime projectile launcher, reusable interaction utility actor, and
  data-driven handling hooks.
- [x] Development-only UE5.8.1 Mac throwable probe for serialized Frag/Veil
  assets, fuse timing, authoritative damage, and suppression; the probe
  explicitly clears the paused mission-menu path before exercising the real
  runtime actors.
- [x] Reload interruption authority path, optional physical-material surface
  rules, and damage-context penetration metadata.
- [x] Character animation state/event interface, optional first-person arms
  slot, and data-asset montage/AnimBP routing.
- [x] Optional weapon surface-impact audio, shell/secondary impact VFX,
  decals/blood gating, and projectile/throwable detonation presentation hooks.
- [x] Optional surface-aware footsteps, low-health/suppression cues, and
  ambient/music/dialogue routing.
- [x] Project-owned low-poly placeholder meshes for the ARX-4, Kestrel-9, Rook
  DMR, Breach-12, Solace pistol, and Arc-7 launcher; each carries authored
  `Grip`/`Muzzle` sockets and the editor automation test recreates, migrates,
  and validates the six assets and runtime muzzle mapping.
- [x] Fresh UE5.8.1 Mac rendered acceptance run confirms the project-owned
  firearm meshes remain visible on AI `hand_r` mounts and fire through the
  authoritative muzzle/tracer path; player first-person weapon presentation is
  visible in the same capture.
- [x] Packaged character-presentation parity: the current cooked package uses
  the project-local `SKM_Manny_Simple` hard cook dependency, project rifle
  locomotion/event clips, and `AnimationSingleNode`; the MoverExamples plugin
  is no longer enabled or force-cooked, so the standalone path cannot silently
  change character source. Evidence: `artifacts/local-manny-showcase-20260803/`
  records the `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple` path for
  every showcase enemy, `hand_r` weapon mounts, movement, and role fire.
- [x] Player-facing reload pacing pass: shortened all six normal/empty profile
  timers (normal `0.35s`–`0.85s`, empty `0.50s`–`1.05s`), stopped held-fire
  input from cancelling reload every tick, preserved visible reload
  feedback/interruption, and regenerated the serialized profiles. The current
  source probe in `artifacts/reload-probe-pacing-source-final-20260803/`
  records the default ARX-4 server timer at `0.45s`, completion restores ammo,
  and the client receives reload start/completion events.
- [x] Actual player reload input path is covered by a development probe:
  simulated `R` is sent through `PlayerController::InputKey`, reaches the
  character action binding, and completes both partial-magazine (`0.50s` actual
  against `0.45s` profile) and empty-magazine (`0.65s`) reloads with ammo
  restored. Evidence: `artifacts/reload-input-probe-20260803/`.
- [x] The final staged Mac package repeats the real `R` input-path probe and
  passes both partial and empty-magazine reloads at `0.45s` and `0.65s` in the
  packaged executable. Evidence:
  `artifacts/package-mac-reload-input-timing-20260803/reload-input-probe/`.
- [x] The fresh goal package repeats the packaged `R` input path at `0.45s`
  normal and `0.65s` empty-magazine timing, and its rendered presentation
  reaches reload start/mid/complete at `0.479s` with host window captures.
  Evidence: `artifacts/package-goal-reload-input-20260804/` and
  `artifacts/package-goal-reload-presentation-20260804/`.
- [ ] Visible reload regression follow-up: the rebuilt rendered showcase now
  records approximately `0.455s` from ARX-4 `ReloadStarted` to
  `ReloadCompleted` in `artifacts/reload-visible-pacing-final-20260803/`.
  The isolated rebuilt packaged showcase also reached the real start, mid,
  and completion states and measured approximately `0.466s` from the weapon
  events (`0.503s` at the showcase polling boundary); its log is in
  `artifacts/package-mac-reload-presentation-probe-20260803c/20260803-140442/`.
  Unreal's packaged screenshot path did not materialize the three requested
  PNGs in that run, so this is timing/presentation-state evidence rather than
  a visual screenshot pass.
  A rebuilt two-client empty-magazine probe now also drives the ordinary
  reload input from `0/150` through `30/120` using the serialized `0.65s`
  ARX-4 empty timer, with client presentation returning to ready; evidence:
  `artifacts/reload-probe-empty-followup-empty-20260803/`. Capture empty
  input-to-ready timing in the rendered game, a player-operated `R` capture, and
  UE5.1/Win64 release behavior before closing the user-facing pacing report.
  The current Mac package containing this pass is
  `artifacts/package-mac-reload-input-timing-20260803/`.
  The verified Mac Pro archive repeats the real input path at `0.46s` partial
  and `0.65s` empty; evidence:
  `artifacts/remote-fresh-probes-20260804/ReloadInputFresh.stdout.log`.
- [x] Enemy return-fire and opening survivability pass: the current encounter
  uses a bounded 120 ms AI decision tick, a 550 ms squad fire budget,
  target-scoped three-round bursts for assault/rifleman roles,
  weapon-rate-aware cooldowns, a total 22-damage hostile shot budget, a
  38-damage hostile explosion cap, a shared 24-damage/1-second target budget,
  and a softer first-wave damage/aim profile.
  The rendered acceptance run at
  `artifacts/return-fire-burst-acceptance-20260803.stdout.log` records 19
  role-fire/animation events across Assault (11), Breacher (2), Rifleman (4),
  and Marksman (2); the corresponding 1280x720 capture is
  `artifacts/return-fire-burst-acceptance-20260803.png`.
- [x] Repeatable packaged enemy-role regression probe: the current staged
  client requires five showcase enemies, four role labels, direct fallback
  movement events, and authoritative role-fire events. Evidence:
  `artifacts/combat-role-probe-followup-20260803/CombatRoleProbe.result.txt`;
  runner:
  `Scripts/run_combat_role_probe.sh`. The isolated Mac Pro package repeated
  the final AI-facing package with five enemies, four roles, five direct
  fallback movement activations, 51 role-fire events, meaningful displacement
  for all five pawns (`124.5`--`434.4 cm`), and zero facing mismatches across
  97 samples; evidence is in
  `artifacts/remote-ai-facing-20260804/CombatRoleLong.stdout.log`. A separate
  28-second remote passive run stayed alive through mission insertion; its
  evidence is `artifacts/remote-ai-facing-20260804/PassiveSurvival.stdout.log`.
  These are compatibility checks, not player-operated survival proof.
- [x] Fresh UE5.8.1 Mac source automation rerun: 33 `BlackMeridian` tests
  succeeded with zero warnings and zero failures; `validate_nightglass.py`
  also passed. Evidence: `artifacts/automation-cinematic-final-20260804/`.
- [x] Final cinematic package revalidation: the local staged app passed the
  five-enemy/four-role combat contract, packaged reload input (`0.45s` normal,
  `0.65s` empty), non-failing Floodline boundary recovery, and the accelerated
  mission clock through `Complete`. The exact package was transferred to the
  Mac Pro at
  `/Users/muhammadluay/Desktop/NightglassSignal-remote-20260804-cinematic-final-seeded/NightglassSignalDemo.app`;
  its executable and cooked-container hashes match the local package, and the
  remote role, boundary, reload, and cinematic probes pass. Evidence:
  `artifacts/package-cinematic-role-probe-20260804/`,
  `artifacts/package-cinematic-boundary-diagnosis-20260804/`,
  `artifacts/package-cinematic-reload-probe-20260804/`,
  `artifacts/package-cinematic-mission-probe-20260804/`, and
  `artifacts/remote-cinematic-final-20260804/`.
- [x] Exact fresh Mac Pro AI-facing package transfer: the four archive parts
  assembled to SHA-256
  `bdd8a42313660657f2b5e3fe534094f0862c5035da3d3e5378946cf9b0a958b4`, and
  the remote executable matched
  `4c059f52feb5cd42399baffe23b9653ca5cfde31b3b4f9620e0e44a1366b8eca`.
  The complete evidence summary is
  `artifacts/remote-ai-facing-20260804/RemoteAiFacingSummary.txt`.
- [x] Development remote-input disconnect guard: `ABMPlayerController` now
  ticks the opt-in bridge while paused, releases every simulated key when the
  client EOF/error is detected, and accepts a replacement client without a
  game restart. The rebuilt UE5.8.1 Mac binary passed the source contract,
  compiled successfully, and the local regression moved W from
  `(0,-950)` to `(390.74,-246.67)` before an abrupt disconnect; the reconnect
  observed stationary `(396.61,-236.11)` at `speed=0.00`. Evidence:
  `artifacts/local-remote-input-regression-20260804/`.
- [x] Source bounded player critical-health recovery: one short recovery
  episode per health cycle, a minimum survival floor, a capped recovery pulse,
  and authoritative hostile damage/death after the episode is consumed. The
  `BlackMeridian.Combat.HostileDamagePacing` suite covers the damage contract;
  the packaged Mac Pro recheck is in
  `artifacts/remote-critical-recovery-20260805/`.
- [x] Mac Pro rebuilt-executable regression: the current Development executable
  was SHA-verified before ad-hoc re-signing and ran from an isolated package
  clone on ports 1992/1993. The SSH/Tailscale bridge released a held W after
  client EOF, accepted a replacement client, and delivered LOOK, fire, and R
  input. Evidence: `artifacts/remote-input-fix-20260804/`. The auto-start
  Escape path returned `handled=yes` but remained `paused=no`, so paused-screen
  recovery is still an explicit follow-up.
- [ ] Player-operated opening-wave survival capture: verify that the current
  package gives the player at least a readable multi-second response window
  while enemies continue to move, hold weapons, and return fire; later-wave
  pressure and the reload pacing report remain separate checks. The new
  bounded critical-recovery pass is verified through the ordinary packaged
  development bridge, but that is not OS-keyboard/player-operated visual proof.
  The fresh controlled package run at
  `artifacts/package-hostile-budget-20260803/combat-showcase-capture/` is
  supporting evidence, and the latest null-renderer package evidence is in
  `artifacts/remote-critical-recovery-20260805/`; neither is counted as
  player-operated input.
- [ ] Production-quality weapon meshes, animation montages, and surface
  physical-material assets.

## Phase 4 — single-player mission

- [x] Operation Glasswake phase director, objective transitions, optional intel,
  subtitles, checkpoint index, extraction, and final defense.
- [x] Non-locking playable intro/extraction/ending camera-cue seam with
  replicated sequence state; authored Level Sequences remain a replacement.
- [x] Friendly formation/assist behavior and runtime squad callouts.
- [x] Ladder input/interaction path, checkpoint SaveGame loadout/transform/
  health/throwable-inventory restore, and reusable interaction action metadata.
- [x] Source revive prompt/restore path and mounted-weapon enter/exit state.
- [x] Runtime mission interaction props for the console, breach, ladder,
  mounted weapon, ammo, Rook pickup, jammer plant, and extraction boat, with
  phase gating and a server-triggered breach barrier/rubble event.
- [x] Persistent five-step mission-flow HUD strip and combat-card layering so
  navigation guidance is replaced by an explicit stay/clear instruction during
  active waves.
- [x] First-minute onboarding clarity pass: the briefing states the immediate
  destination, the first objective marker renders above the briefing layer,
  and the persistent route strip includes an explicit colour/action legend.
- [x] Development-only UE5.8.1 Mac interaction probe drives the ordinary hold
  path for relay breach, optional archive console, and extraction boat through
  authoritative mission completion.
- [x] Development-only UE5.8.1 Mac interaction acceptance probe verifies the
  runtime cover query, mounted-weapon enter/exit, replicated destruction event,
  and ordinary revive hold/restore path; player-facing throwable presentation
  is also captured for both Frag and Veil through the real throw path;
  authored cinematic presentation remains open.
- [x] Checkpoint restart world reset for stale enemy waves, friendlies, mission
  interactables, kill-feed state, and replicated breach environment.
- [x] Development-only two-process UE5.8.1 Mac persistence probe for isolated
  checkpoint/profile serialization and restart-from-checkpoint restoration.
- [x] Development-only accelerated mission pacing probe logs the real phase
  clock through Complete; authored playable cinematics, destruction set piece,
  and authored mounted weapon/revive presentation remain open.
- [ ] Player-captured 10–15 minute pacing and authored presentation pass.

## Phase 5 — multiplayer

- [x] Replicated player/game state, team assignment, control nodes, rotation,
  scoring, scoreboard/result hooks, server target, and mode travel entry.
- [x] Match countdown, configurable respawn, dynamic 12-slot bot fill, and
  spawn selection scored against visible enemies and recent combat locations;
  startup fill waits for valid arena starts and equips each bot.
- [x] Runtime lobby entry, replicated two-column roster (host/team/bot state),
  host/server travel, lobby start request, countdown, kill feed, team-preserving
  bot respawn, and recent-combat spawn guard.
- [x] Live-match disconnect handling with authority-side same-team bot takeover
  using the recent-combat/visibility-safe spawn selector.
- [x] Sixteen competitive perimeter starts prevent a joining human from
  colliding with the temporary 12-slot bot fill before bot trimming.
- [x] Bounded server requests for sprint, tactical sprint, aim, and prone state
  so CharacterMovement prediction has an authority-side speed/state contract.
- [x] Player-owned hitscan requests are rejected when the submitted ray is
  more than 25 degrees from the replicated controller view; AI/server-owned
  rays remain authoritative.
- [x] Server-awarded per-player objective points for active-node occupancy.
- [x] Development-only UE5.8.1 Mac host/two-client probe for login/team
  assignment, countdown/live state, node rotation, and server scoring.
- [x] Bounded authority-side hitscan rewind using half the reported RTT, a
  250 ms maximum, stance-aware pose history, and a two-client packet-emulation
  probe that also verifies the existing disconnect bot takeover.
- [ ] Authored UMG/production lobby treatment, 2–12-player soak, release-target
  packet-loss behavior, per-bone/anti-cheat validation beyond the current
  25-degree view-cone guard, and
  release-target disconnect QA. The current runtime roster is presented by
  `UBMNativeUIWidget`; authored Widget Blueprints remain the production
  replacement.

## Phase 6 — polish and release

- [x] Setup/build/test scripts, QA checklist, attribution, known issues,
  roadmap, performance template, and trailer checklist.
- [x] Persisted settings navigation for FOV, independent sensitivity, input
  toggles, accessibility, audio values, friendly-fire policy, scalable graphics
  presets, and keyboard binding presets.
- [x] Conflict-safe keyboard preset remapping rejects reserved menu keys,
  swaps collisions between remappable actions, preserves gamepad bindings, and
  rebuilds the live key map; UE evidence:
  `artifacts/automation-remap-final-20260803/`.
- [x] Heading-aware compass HUD and automation contracts for keyboard/controller
  bindings plus AI perception initialization.
- [x] Runtime-authored `UBMNativeUIWidget` UMG surface for menu, campaign,
  lobby, settings, controls, How To Play, pause, HUD, scoreboard, death, and
  results states; the legacy canvas path remains a compatibility fallback.
- [x] Runtime-authored front-end controller navigation: D-pad selection,
  controller-A activation, and controller-B back use the same menu action
  wrappers as mouse/keyboard input; player-visible/release-target review
  remains open.
- [x] Development-only local ammo/restore cheat commands are compiled as
  shipping no-ops.
- [x] UE5.1/Win64 BuildCookRun wrapper and package verifier now exist as
  `Scripts/build_windows.bat` and `Scripts/verify_windows_package.ps1`; the
  actual Windows package/runtime gate remains open until they run on the
  provisioned target.
- [x] Development-only rendered frame-time probe records average FPS, P95
  frame time, maximum frame time, and 60 FPS budget status at 1080p.
- [x] UE5.8.1 Mac editor/game targets, 30-test automation, cooked/staged
  client, authored-map package smoke for both `Glasswake` and `SignalControl`,
  and staged-app null-renderer evidence are recorded. The clean audio/UMG
  automation rerun is `artifacts/automation-combat-audio-20260804-clean/`;
  the package is not a Win64/UE5.1 release substitute.
- [ ] Windows packaged development build, 1080p/60 FPS measurement, authored
  UI/audio/VFX, accessibility visual review, arbitrary-key/full-UMG remapping,
  and release capture.
