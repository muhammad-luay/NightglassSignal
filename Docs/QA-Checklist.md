# QA checklist

Status uses `source`, `engine`, `runtime`, or `open`. `source` means the
current repository contains the implementation seam; it is not proof of a
tested UE runtime.

## Core and combat

- [x] `source` movement actions and CharacterMovementComponent path exist.
- [x] `source` sprint/tactical sprint/crouch/prone/slide/lean and
  server-authoritative mantle request/validation.
- [x] `source` data-driven fire modes, recoil, ammo, reload, weapon switching.
- [x] `engine` serialized weapon and throwable profiles load from
  `Content/Data` and retain runtime fallbacks; editor automation result:
  `artifacts/automation-throwable-data-assets-rerun-20260802/`.
- [x] `engine` fresh UE5.8.1 Mac development build resolves all six
  project-owned firearm meshes, including their authored `Grip`/`Muzzle`
  sockets, anchors the authored `Grip` to each enemy's `hand_r`, and fires from
  the authored `Muzzle`; the live showcase shows the player weapon,
  enemy-held weapons, muzzle flashes, and tracers; evidence:
  `artifacts/automation-weapons-fresh-20260802/index.json`,
  `artifacts/weapon-showcase-visible-window-20260802.png`, and
  `artifacts/weapon-showcase-fresh-20260802.log`. The grip regression check is
  also covered by `artifacts/automation-weapon-grip-assertion-20260802/`, with
  live mount logs and capture in `artifacts/weapon-grip-live-20260802/`.
- [x] `source` suppressed/unsuppressed fire routing, suppressor attachment
  metadata, and reduced AI hearing noise for suppressed shots.
- [x] `source` reload interruption, optional montage/event animation contract,
  first-person arms slot, and weapon audio/VFX routing guards.
- [x] `source` surface-specific impact audio, shell/secondary impact VFX,
  optional decals/blood gore gating, detonation presentation, and local
  suppression post-process response.
- [x] `source` optional surface-aware footsteps, low-health/suppression cues,
  ambient/music/dialogue routing, and playable camera-cue presentation.
- [x] `engine` UE5.8.1 Mac sound-enabled mission run registers CoreAudio and
  drives the procedural music states through `Mission -> Suspense -> Combat ->
  Suspense -> Combat -> Complete`; evidence:
  `artifacts/suspense-music-sequence-20260801.log`. The separate authored
  Sound/Niagara asset gate below remains open.
- [x] `source` hitscan, pellet support, falloff, hit zones, headshot multiplier.
- [x] `source` readable hostile-fire budget and opening-wave damage/aim tuning;
  `BlackMeridian.Combat.HostileDamagePacing` guards the 550 ms squad slot,
  opening start delay, role aim error, maximum opening torso volley, and the
  shared 24-damage/1-second target budget across hostile weapons and ordnance.
- [x] `source` bounded critical-health recovery keeps one hostile-damage
  episode from deleting the player immediately, while preserving an explicit
  minimum floor, recovery cap, authoritative damage during the pulse, and
  normal death after the episode is consumed.
- [x] `runtime` exact staged Mac Pro package recheck reaches the five-hostile
  checkpoint, accepts player fire/reload through the loopback bridge, records
  magazine `30 -> 0` and reload restoration to `30/180`, and logs the
  critical-recovery pulse at health `28.1`; evidence:
  `artifacts/remote-critical-recovery-20260805/`. This is null-renderer bridge
  evidence; OS-keyboard/player-operated visual survival remains open.
- [x] `runtime` the follow-up exact cap-6 Mac Pro package matches all six
  executable/cooked-container hashes, passes deep strict codesign, reaches
  phase 6/OpenCombat with eight hostiles, and records stationary health
  observations `79.4 -> 42.9 -> 21.5 -> 28.9` without elimination across
  roughly 130 simulated combat seconds. The ordinary reload path restores
  `0/210` to `30/180`; evidence:
  `artifacts/remote-player-survival-cap6-20260805/`. This is development
  bridge/null-renderer evidence and does not close the player-operated gate.
- [x] `source` launcher projectile path, shot-spread penalty/recovery, and
  data-driven procedural first-person weapon sway.
- [x] `source` ThinCover penetration tag and damage context.
- [x] `source` physical-surface rule lookup with greybox tag fallback and
  penetration count metadata.
- [x] `source` replicated health, regeneration policy, death event.
- [x] `source` server-authoritative frag/Veil inventory, fuse/detonation, and
  tactical suppression path.
- [x] `source` player-owned fire rays are bounded to a 25-degree controller-view
  cone on authority; AI/server-owned fire remains explicit and authoritative.
- [x] `engine` rebuilt UE5.8.1 Mac development throwable probe spawns the
  serialized Frag and Veil profiles, verifies 2.40s/1.60s fuse timing,
  authoritative frag damage without elimination, and Veil suppression;
  evidence: `artifacts/throwable-probe-current-20260802d/`.
- [x] `engine` UE5.8.1 Mac development and packaged combat-feel probes exercise
  the real player weapon path: ARX-4 first shot accepts (`30->29`), immediate
  and half-interval requests are rejected, the post-interval request accepts,
  accuracy penalty rises `0.000->0.350` and recovers to `0.000`, and the
  first-person weapon root moves under procedural sway (`0.345 cm` /
  `0.341 deg` in the packaged run). The isolated Mac Pro package repeats the
  same path with `0.339 cm` / `0.351 deg` sway and a `PASS` result. Evidence:
  `artifacts/combat-feel-probe-local/` and
  `artifacts/combat-feel-probe-packaged-local/` and
  `artifacts/combat-feel-probe-remote-20260804/`.
- [ ] `engine` rendered/player-visible sway readability, reload-interruption
  behavior, authored animation sync, and audio/VFX playback with real
  Sound/Niagara/material assets.
- [x] `engine` UE5.8.1 Mac compatibility evidence covers the current ARX-4
  reload path: the rebuilt two-client source probe drives reload input through
  weapon-ready with replicated ammo/feedback and confirms the regenerated
  serialized `0.45s` normal timer plus completion/ammo restoration; evidence:
  `artifacts/reload-probe-pacing-source-final-20260803/`. The older reload
  packages are excluded from current playtest evidence. The fresh rendered
  showcase also records approximately `0.455s` from `ReloadStarted` to
  `ReloadCompleted` in `artifacts/reload-visible-pacing-final-20260803/`.
  The newer isolated packaged showcase records the same event path in about
  `0.466s` (the polling marker reports `0.503s`); log evidence is in
  `artifacts/package-mac-reload-presentation-probe-20260803c/20260803-140442/`.
- [x] `engine` rebuilt UE5.8.1 Mac two-client empty-magazine probe drives the
  ordinary reload input after replicated `0/150` ammo preparation, verifies the
  serialized ARX-4 `0.65s` empty timer, restores `30/120`, and observes client
  reload start/completion plus the return to ready; evidence:
  `artifacts/reload-probe-empty-followup-empty-20260803/`.
- [x] `runtime` current staged audio/native-UMG Mac package repeats the real
  `R` input path at `0.45s` partial and `0.65s` empty completion; evidence:
  `artifacts/package-mac-audio-20260804/frozen-reload-probe/`.
- [ ] `engine` capture empty reloads and every selectable profile from input
  through weapon-ready, including authored animation timing, interruption, and
  UE5.1/Win64 release-target behavior; current source data spans `0.35s`–`0.85s`
  normal and `0.50s`–`1.05s` empty reloads across the six profiles. An
  interactive `R` capture is still required because the automated showcase
  and source probe do not establish every input/animation presentation path;
  the latest packaged screenshot requests also failed to materialize PNG files,
  so the timing log is not being counted as visual screenshot evidence.
- [x] `engine` UE5.8.1 Mac development combat probe drives a remote client
  through the ordinary player fire path and verifies server shot acceptance,
  authoritative ammo decrement, server damage, and replicated ammo/health.
- [x] `engine` UE5.8.1 Mac Signal Control probe completes login, countdown,
  live match, scoring, and node rotation with Unreal packet emulation at
  `PktLag=100` and `PktLoss=5`; see
  `artifacts/signal-control-probe-packet-loss/`.
- [ ] `engine` full release-target combat packet-loss behavior, reload
  synchronization, and lag-sensitive/lag-compensated combat behavior. The
  bounded half-RTT rewind and player-view cone are covered by the current
  development probe; release-target and per-bone validation remain open.
- [x] `source` repeatable rendered performance probe records average/P95 frame
  time and 60 FPS budget status; target Windows hardware measurement remains
  open.
- [x] `engine` clean isolated rendered performance result. The fresh packaged
  Mac probe at `artifacts/performance-probe-clean-20260803/` ran without
  competing Nightglass processes at an actual 1440x1080 viewport and recorded
  72.15 FPS average, 14.33 ms P95, 69.78 P95 FPS, and
  `target_60fps_p95=yes`. This is compatibility evidence only; the required
  Windows mid-range 1080p result remains open.

## AI and mission

- [x] `source` sight perception, reaction delay seam, target memory, patrol,
  investigation, engagement, flank movement, aggression tuning.
- [x] `engine` UE5.8.1 Mac rendered combat showcase verifies five authored
  Manny enemies advance from their spawn positions, select role weapons, display
  the third-person weapon silhouette from the `hand_r` mount, and fire from the
  authored mesh muzzle; the rebuilt fresh-run evidence is
  `artifacts/weapon-showcase-visible-window-20260802.png` and
  `artifacts/weapon-showcase-fresh-20260802.log`.
- [x] `engine` packaged character presentation is deterministic in the current
  package: every showcase enemy loads the project-local Manny mesh, uses the
  project rifle clips, and mounts a visible weapon at `hand_r`; evidence:
  `artifacts/local-manny-showcase-20260803/NightglassLocalManny.stdout.log`.
- [x] `engine` enemy return-fire cadence is readable in the current packaged
  encounter: sustained role-appropriate burst events and weapon mounts are
  recorded in `artifacts/local-manny-showcase-20260803/`; production audio/VFX
  and UE5.1/Win64 release validation remain open.
- [x] `engine` repeatable packaged combat-role probe requires five enemies,
  Assault/Breacher/Rifleman/Marksman role coverage, direct fallback movement,
  and authoritative role-fire events; evidence:
  `artifacts/combat-role-probe-followup-20260803/CombatRoleProbe.result.txt`.
  The latest AI-facing exact fresh Mac Pro package repeated the contract with
  five enemies, four roles, five movement activations, 51 role-fire events,
  meaningful displacement for all five pawns, and zero facing mismatches
  across 97 samples; evidence:
  `artifacts/remote-ai-facing-20260804/CombatRoleLong.stdout.log`.
- [x] `runtime` current staged audio/native-UMG Mac package repeats the role
  contract with five enemies, four roles, movement events, and live role-fire;
  evidence: `artifacts/package-mac-audio-20260804/frozen-combat-role-probe/`.
- [x] `runtime` the AI-facing exact package stayed alive for 28 seconds through
  Glasswake insertion with no `SQUAD LOST`/elimination signal; evidence:
  `artifacts/remote-ai-facing-20260804/PassiveSurvival.stdout.log`. This does
  not count as player-operated survivability evidence.
- [x] `runtime` the final checksum-matched Mac Pro cinematic package repeats
  five enemies/four roles, five movement activations, 22 role-fire events,
  meaningful displacement for all five pawns, and zero facing mismatches across
  70 samples; evidence:
  `artifacts/remote-cinematic-final-20260804/CombatRole.stdout.log`.
- [ ] `engine` player-operated opening-wave survivability capture on the
  exact 2026-08-05 arm64 Mac Pro package. The controlled restart reached
  health 19 and then health 0 with two hostiles still active, so the balance
  gate remains open. Evidence: artifacts/remote-opening-survival-state-20260805/.
- [x] `source` friendly follow, independent visible-enemy engagement, assist,
  formation, objective anchors for breach/advance/defense, thin-cover movement,
  subtitle callout, and timed revive prompt/restore path.
- [x] `source` Operation Glasswake mission phases and optional intel objective.
- [x] `source` runtime greybox insertion, checkpoint, relay, arena, pier.
- [x] `source` non-failing playable bounds: four hidden, tall perimeter
  colliders plus last-safe-ground recovery keep edge traversal on the floor;
  `BlackMeridian.World.PlayableBoundary` guards the wall contract.
- [x] `engine` packaged Floodline boundary-guard probe drives the player into
  the outer perimeter and records `location=X=-2065.943 Y=-900.000 Z=85.150`,
  `grounded=yes`, and `eliminated=no`, with no mission failure or `SQUAD LOST`;
  evidence:
  `artifacts/boundary-guard-packaged-20260803/BoundaryGuardProbe.stdout.log`.
  The assembled four-part Mac Pro package repeats the guard at
  `location=X=-2066.000 Y=-900.000 Z=85.150`; evidence:
  `artifacts/remote-macos13-boundary-20260804/NightglassRemoteMac13Boundary.log`.
- [x] `runtime` current staged audio/native-UMG Mac package repeats the
  boundary guard with `grounded=yes` and `eliminated=no`; evidence:
  `artifacts/package-mac-audio-20260804/frozen-boundary-probe/`.
- [x] `source` twenty selectable campaign operations with distinct route
  anchors, palette treatments, starts, objective locations, pickup choices,
  and encounter budgets. These are runtime-generated Entry-map variants, not
  twenty authored `.umap` files.
- [ ] `engine` packaged runtime exposes `LEVEL 1/20` through `LEVEL 20/20`,
  cycles the complete selector, and smoke-tests representative operations;
  source coverage is complete, but a fresh UE runtime/package pass remains
  required.
- [x] `source` reusable tap/hold interaction actions, ladder movement, pickup
  hooks, mounted-weapon state, and checkpoint transform/health/loadout/
  throwable-inventory restoration.
- [x] `engine` weapon and ammunition pickups auto-collect within 180 cm;
  weapon pickups grant the firearm and bonus reserve ammo, equip it
  immediately, show the acquisition message, and destroy the pickup. The
  source contract covers all twenty campaign layouts; a fresh packaged
  pickup probe across representative levels remains required.
- [x] `source` phase-gated console, breach, jammer, and extraction
  interactions plus runtime mission props and barrier destruction event.
- [x] `source` checkpoint restart reset destroys stale waves, restores friendlies
  and mission props, clears combat counters, and restores the breach barrier on
  clients through a reliable multicast.
- [x] `automation` mission interaction phase gates and checkpoint jammer
  persistence contracts are covered in `BlackMeridianTests`.
- [x] `engine` UE5.8.1 Mac development interaction probe completes the real
  relay breach, optional archive-console hold, extraction-boat hold, final
  defense, and mission completion. Evidence:
  `artifacts/interaction-probe-onboarding-legend-20260801/` and the fresh
  current-source rerun `artifacts/interaction-clean-20260804/`.
- [x] `runtime` the final local and exact Mac Pro packages pass the playable
  boundary contract (`X=-2066.000`, grounded, not eliminated), the real `R`
  reload path (`0.46s` normal and `0.65s` empty on the Mac Pro), and the
  accelerated mission clock through `Complete`, including Intro, SetPiece,
  Extraction, and Ending cues. Evidence:
  `artifacts/remote-cinematic-final-20260804/` and the matching local package
  probe artifacts.
- [x] `engine` UE5.8.1 Mac interaction acceptance probe verifies runtime cover
  selection, mounted-weapon enter/exit, the replicated destruction event, and
  the ordinary revive hold/restore path; evidence:
  `artifacts/interaction-acceptance-probe-20260802/`.
- [x] `engine` UE5.8.1 Mac rendered first-person Frag and Veil throw
  presentation uses the project meshes, hides the current firearm during the
  hand/release motion, and calls the ordinary authoritative throw path;
  evidence: `artifacts/throwable-presentation-live-20260802/frag.png`,
  `veil.png`, and their matching logs.
- [ ] `engine` authored mission cinematics and final presentation. The
  development throwable probe above covers the authoritative
  fuse/damage/suppression contract; the rendered throw capture does not prove
  authored Level Sequences, VO, or destruction timing.
- [x] `engine` UE5.8.1 Mac two-process persistence probe writes an isolated
  checkpoint/profile slot, restores phase, transform, health, loadout, active
  weapon, throwable inventory, optional state, and profile unlocks in a fresh
  process, then passes the real restart-from-checkpoint path; evidence:
  `artifacts/persistence-probe/PersistenceWrite-20260801-183928-26156.log` and
  `artifacts/persistence-probe/PersistenceRead-20260801-183928-26156.log`.
- [ ] `runtime` complete 10–15 minute paced mission capture.

## UI/accessibility

- [x] `source` splash-equivalent/menu, settings navigation, controls, pause, HUD,
  scoreboard, death, and results screens.
- [x] `source` objective marker text, health, ammo, weapon, hostiles, hit marker,
  damage direction, subtitle, and latency data seam.
- [x] `engine` UE5.8.1 Mac interactive launch survives delayed HUD
  initialization: main menu -> Campaign -> Deploy stays in `InGame`, the
  opening instruction card is visible, movement reaches the first beacon, and
  the checkpoint card explains the five-hostile combat step. Evidence:
  `artifacts/interactive-proof/main-menu.png`,
  `artifacts/interactive-proof/how-to-play.png`, and
  `artifacts/interactive-proof/checkpoint-combat-card.png` from the
  2026-08-01 probe.
- [x] `engine` UE5.8.1 Mac rendered launch logs the live 1600x900 HUD with the
  mission in `InGame`; the foregrounded capture
  `artifacts/rendered-clarity-live-20260801.png` now verifies the opening
  mission card, the non-overlapping five-step `MOVE / CLEAR / BREACH /
  EXTRACT / DEFEND` route strip, and the initial objective marker. Earlier
  captures
  `artifacts/rendered-guidance-final-20260801.png`,
  `artifacts/rendered-guidance-final-combat-20260801.png`, and
  `artifacts/rendered-guidance-final-combat-live-20260801.png` verify the
  persistent phase-specific action line, combat
  stay/clear wording, and non-overlapping phase-card presentation. The live
  development interaction probe verifies hold timing and authoritative
  completion; a dedicated rendered hold capture is still open.
- [x] `engine` UE5.8.1 Mac onboarding rerun verifies the first objective marker
  remains visible above the opening card and the route strip explicitly labels
  `TEAL MOVE / AMBER FIGHT / ORANGE HOLD E / YELLOW EXTRACT`; evidence:
  `artifacts/onboarding-legend-live-20260801.png`.
- [x] `engine` fresh packaged Mac launch makes the reliable Controls shortcut
  explicit in the main menu (`K CONTROLS`, `F2: Fn+F2 on Mac`), and the
  rebuilt package opens Controls through both `K` and a function-modified F2
  event; evidence: `artifacts/f2-robust-k-live-20260802.png` and
  `artifacts/f2-robust-fn-f2-window-20260802.png`. Bare F2 remains dependent
  on the host macOS function-row setting.
- [x] `source` keyboard binding preset selection persists through the legacy
  input map and leaves controller mappings available.
- [x] `source` keyboard preset remapping rejects reserved menu keys, swaps
  colliding remappable actions atomically, and preserves every gamepad
  mapping; UE automation evidence:
  `artifacts/automation-remap-final-20260803/index.json`.
- [x] `source` runtime-authored menu buttons expose controller navigation:
  D-pad actions move the highlighted action, controller A invokes the same
  action as a click/Enter, and controller B returns through the existing Back
  path. The input contract is covered by the isolated UE5.8.1 automation rerun
  (`artifacts/automation-controller-menu-20260804/index.json`, `32/32`
  successful, zero warnings); release-target and player-visible
  controller review remain open.
- [x] `source` heading-aware compass with cardinal/intercardinal labels and
  numeric heading feedback; keyboard/controller action bindings and AI
  perception initialization have automation coverage.
- [ ] `engine` visual review at 1080p, controller navigation, hold/toggle
  options, remapping conflict review, color-blind presets, and audio slider
  verification, including authored footsteps and cue assets.

## Multiplayer

- [x] `source` dedicated-server target, listen-server-compatible actor classes,
  team assignment, three nodes, rotation, scoring, scoreboard/result hooks, and
  a replicated lobby roster with host/team/bot/capacity state.
- [x] `source` server-awarded per-player objective score for active-node
  occupancy.
- [x] `source` bots, countdown, respawn delay, recent-combat/visibility spawn
  scoring, kill-feed state, live-match same-team bot takeover, latency data
  seam, and game-mode travel/lobby start request.
- [x] `engine` UE5.8.1 Mac listen-server probe verifies two remote client logins
  with Lattice/Vanta assignment, the authoritative countdown, match-live
  transition, active-node rotation, and a real server-side score tick; this is
  development probe evidence, not a certified 2–12 player release session.
- [x] `engine` UE5.8.1 Mac Signal Control results probe verifies the authoritative
  winner and both replicated team scores reach the results screen; the recorded
  development run is Lattice 2–0 in `artifacts/signal-results-probe/`.
- [x] `engine` UE5.8.1 Mac development combat probe verifies a two-remote-client
  fire/damage replication path and intentionally disconnects the probe-owned
  shooter to observe server-side bot takeover.
- [x] `engine` UE5.8.1 Mac reload input probe sends `R` through
  `PlayerController::InputKey`, confirms the character reload action binding,
  and restores ARX-4 ammo for both partial and empty magazines at the expected
  short timers; evidence: `artifacts/reload-input-probe-20260803/`.
- [x] The archived staged Mac executable repeats the reload probe and records
  `0.45s` partial-magazine and `0.65s` empty-magazine completion, with the
  reload binding invoked and ammo restored; evidence:
  `artifacts/package-mac-reload-input-timing-20260803/reload-input-probe/`.
- [x] `engine` UE5.8.1 Mac combat probe with `PktLag=100` and `PktLoss=5`
  verifies the bounded authority-side hitscan rewind path and the existing
  disconnect bot takeover; the host records `ping_ms=204.3`,
  `rewind_ms=103.3`, and `targets=6` in
  `artifacts/combat-rewind-probe-lag-verified-20260802/CombatHost-20260802-144452.log`.
- [x] `engine` UE5.8.1 Mac Signal Control startup probe fills the remaining
  12-player slots with balanced bots at valid arena starts, and every startup
  bot reports a held weapon; evidence:
  `artifacts/signal-control-12slot-startup-20260802/`.
- [x] `engine` final rebuilt-module Signal Control probe supplies sixteen
  perimeter starts, admits two remote clients before bot trimming, and records
  zero spawn-collision or live-player-recovery warnings while preserving
  live scoring and node rotation; evidence:
  `artifacts/signal-control-final-20260802/`.
- [x] `engine` fresh UE5.8.1 Mac Signal Control rerun on the current source
  reaches live state, fills 11 bots for one human with balanced 6/6 teams,
  scores the active node, rotates nodes, and trims bots as two more humans
  join; evidence: `artifacts/signal-control-clean-20260804/`.
- [ ] `engine` 2–12-player session, authored lobby, broader lobby/disconnect
  recovery, release-target packet-loss behavior, per-bone/anti-cheat
  validation beyond the current 25-degree view-cone guard, and release-target
  lag-compensation QA. CharacterMovement
  prediction/interpolation/reconciliation and bounded sprint/aim/prone state
  requests remain compatibility-runtime gates.

## Release and performance

- [x] runtime the exact 2026-08-05 arm64 package was checksum verified on the
  Mac Pro and exercised through a held loopback bridge session. W moved the
  player into the checkpoint; live STATE proved weapon, magazine, reserve,
  reload, phase, health, and hostile fields; and the remote log recorded five
  hand-mounted enemy rifles, changing weapon mounts, enemy fire, reload, and
  hit reactions. Evidence:
  artifacts/remote-opening-survival-state-20260805/.

- [x] `runtime` the later cap-6 package was checksum verified on the Mac Pro,
  launched on bridge port `3013`, and kept alive in phase 6/OpenCombat with
  eight hostiles while the reduced damage-window cap was exercised. Evidence:
  `artifacts/remote-player-survival-cap6-20260805/`.

- [x] `source` setup/build/automation scripts and asset attribution boundary.
- [x] `source` no external art/audio claims and no copied third-party identity.
- [x] `engine` UE5.8.1 Mac editor target and game target compile; 33/33
  `BlackMeridian` automation tests pass with zero warnings or failures,
  including campaign-level, campaign-progress, proximity-pickup, reload-profile,
  AI perception, and controller-input contracts;
  Entry-map greybox smoke spawns 18 runtime pieces, 8 mission interactables,
  and both directors. Fresh evidence:
  `artifacts/automation-remap-final-20260803/`,
  `artifacts/automation-combat-audio-20260804-clean/`, and the final rerun
  `artifacts/automation-cinematic-final-20260804/`.
- [x] `engine` UE5.8.1 Mac client cooks and stages successfully; the latest
  rebuilt staged app archive mounts the cooked containers, loads both authored
  map shells (`Glasswake` and `SignalControl`), and exits cleanly under
  `-nullrhi`. Evidence: `artifacts/package-mac-reload-input-timing-20260803/`
  and `artifacts/package-mac-reload-input-timing-20260803/staged-map-smoke/`.
- [x] `engine` UE5.8.1 development mission pacing probe runs the real phase
  clock from Insertion through Complete at 615.25 simulated seconds
  (10.25 minutes); the rebuilt package also records the three-cue ending and
  deferred results screen in
  `artifacts/packaged-mission-presentation-probe-20260803.log`. This is timing
  evidence, not a player-captured mission presentation pass.
- [x] `runtime` the four remote archive parts were assembled on the Mac Pro,
  matched SHA-256
  `d107c1fbbc5d475a3b46e82d14b2a6db13b963bfe38ae020255ab16b1761d0e1`, and
  the macOS 13 compatibility package launched through Metal; evidence:
  `artifacts/remote-macos13-visual-20260804/NightglassRemoteMac13Visual.log`.
- [x] `runtime` the isolated Mac Pro packaged combat-feel probe passed after
  disabling the missing runtime-only `AvfMedia` module; the arm64 executable
  passed deep code-signature verification and the remote log records the real
  player fire/ammo, interval, accuracy, and sway measurements. Evidence:
  `artifacts/combat-feel-probe-remote-20260804/`.
- [x] `runtime` the newly transferred Mac Pro development package was checksum
  verified remotely, mounted all four cooked containers, and loaded Glasswake
  through the loopback-only `-BM_RemoteInputPort=1986` bridge. A real `W` hold
  moved the pawn from `x=0.00 y=-950.00` to `x=243.71 y=-511.32` at
  `360 cm/s`; LOOK, LMB, and R were accepted by the ordinary controller input
  path. This is headless compatibility evidence, not player-operated or
  full-screen capture; the render-enabled run remained under host I/O pressure.
- [x] `runtime` the fresh AI-facing rendered package accepted `PING`, `STATE`,
  `LOOK`, `W DOWN/UP`, and `LMB DOWN` through the existing loopback SSH
  forward. A held `W` moved the pawn from `x=0.00 y=-950.00` to
  `x=800.56 y=-280.65`; the rendered log contains five enemy spawns, 18 AI
  movement samples, 59 HUD frames, and 506 first-person dry-fire events.
  Evidence: `artifacts/remote-ai-facing-20260804/`. The final release/state
  response was lost to a stale client connection, so player-operated and
  full-screen capture remain open.
- [x] `engine` the rebuilt UE5.8.1 Mac development bridge detects an EOF/error
  client state, releases all tracked simulated keys (including held W),
  releases the single-client slot, and accepts a subsequent client. The local
  replacement-client regression moved the pawn while W was held, disconnected
  without `KEY UP`, then observed stationary `speed=0.00`; evidence:
  `artifacts/local-remote-input-regression-20260804/RemoteInputDisconnect.stdout.log`.
  The earlier remote EOF/client-slot proof remains in
  `artifacts/remote-input-bridge-eof-20260804/`.
- [x] `runtime` the current rebuilt executable was verified and exercised in an
  isolated Mac Pro clone on ports 1992/1993. The remote bridge released held W
  after EOF, accepted a replacement client at `speed=0.00`, and accepted LOOK,
  fire, and reload commands; evidence: `artifacts/remote-input-fix-20260804/`.
  Escape was handled but did not produce `paused=yes` in the auto-start run;
  paused-screen recovery remains open.
- [x] `runtime` PID-scoped player-visible Mac Pro gameplay captures show the
  environment, HUD, first-person weapon, enemies, movement, reload, and muzzle
  fire; evidence:
  `artifacts/remote-authored-reload-20260804/` and
  `artifacts/remote-rendered-authored-20260804/`. OS full-screen/trailer
  capture remains permission-limited; see [Known Issues](Known-Issues.md#known-issues),
  item 20.
- [ ] `engine` UE5.1/Win64 compile and packaged development build, 1080p/60
  FPS mid-range measurement, Insights trace, soak test, and trailer capture.

## Defect recording

Record each failed item with commit, UE version, platform, map/mode, exact
steps, expected authority, observed result, log excerpt, and a regression note
in [Known Issues](Known-Issues.md). Never convert an unchecked item into a
green result because the code path looks plausible.
