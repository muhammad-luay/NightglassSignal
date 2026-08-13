# Black Meridian requirement audit

Last reviewed: 2026-08-05. This audit separates source/runtime evidence from
release gates; a passing compatibility probe is not treated as UE5.1/Win64 or
player-operated proof.

| Specification area | Current status | Evidence / remaining gate |
| --- | --- | --- |
| Original first-person military identity | Pass | Black Meridian / Operation Glasswake / Signal Control are original project terminology and documented in `Docs/Game-Design-Document.md`. |
| Modular first-person movement and combat | Compatibility pass | Runtime components, data-driven six-firearm set, throwables, recoil, sway, reload, damage zones, falloff, and authority checks; fresh staged `Glasswake`/`SignalControl` map smoke, packaged reload input (`0.45s` normal / `0.65s` empty), and boundary guard probes pass in `artifacts/package-goal-staged-map-smoke-20260804/`, `artifacts/package-goal-reload-input-20260804/`, and `artifacts/package-goal-boundary-guard-20260804/`. UE5.1/Win64 runtime QA remains open. |
| Enemy AI and visible weapons | Compatibility pass | The final local package role probe records five enemies, four roles, five direct movement activations, 48 role-fire events, and zero facing mismatches. The exact final Mac Pro package repeats five enemies, four roles, five movement activations, 22 role-fire events, meaningful displacement for all five pawns (maximum `424.4 cm`), and zero facing mismatches across 70 samples in `artifacts/remote-cinematic-final-20260804/CombatRole.stdout.log`. These are compatibility/null-renderer evidence, so player-operated turning/survival review and authored production animation/AI assets remain open. |
| Friendly squad and interactions | Compatibility pass | Fresh interaction probe completes relay breach, archive console, extraction, final defense, and mission completion; `artifacts/interaction-clean-20260804/`. |
| Single-player mission | Source/runtime pass, player presentation open | The campaign source now defines twenty runtime operations (`SableQuay` through `LastLight`) with distinct route anchors, starts, palette treatments, pickups, and encounter budgets. Existing checkpoint/profile restoration, non-failing boundary recovery, and accelerated interaction evidence remain valid for the shared mission flow. A fresh packaged 1/5/10/15/20 smoke, player-operated 10–15 minute run, authored Level Sequences, and final set-piece presentation remain open. |
| Multiplayer Signal Control | Development compatibility pass | Current source probe reaches live state, fills 11 bots for one human with balanced teams, scores, rotates nodes, and trims bots as humans join; `artifacts/signal-control-clean-20260804/`. A real 2–12-player soak, release packet-loss, dedicated-server, and anti-cheat QA remain open. |
| UI and controls | Source/native-UMG pass, visual review open | `UBMNativeUIWidget` covers menu, campaign, lobby, settings, controls, How To Play, pause, HUD, scoreboard, death, and results. Runtime source now supports D-pad menu selection, controller A activation, and controller B back; 1080p visual review, color-blind/hold-toggle review, release-target controller QA, and final authored styling remain open. |
| Audio and VFX | Compatibility placeholder pass | Generated WAV/SoundWave and Niagara placeholder assets are routed and present in the fresh staged package; the packaged reload presentation reaches start/mid/complete at `0.479s` and captures the running game window in `artifacts/package-goal-reload-presentation-20260804/`. The fresh source automation is 33/33 with zero warnings/failures. Final authored/licensed mix and presentation review remain open. |
| Saves, accessibility, and settings | Source pass, release review open | Settings persistence, checkpoint/profile serialization, remapping guards, subtitle/color/motion/audio options exist and have automation/probe coverage. UE5.1/Win64 migration and visual/controller review remain open. |
| Performance target | Mac compatibility evidence only | A fresh 30-second rendered run on the remote macOS 13.0.1 Apple M1 Pro recorded `avg_fps=28.19`, `p95_frame_ms=37.72`, `p95_fps=26.51`, and `target_60fps_p95=no`; Windows mid-range 1080p/60, scalability matrix, Insights trace, and multiplayer soak remain open. |
| Build/release | Mac/UE5.8.1 pass, intended release open | Fresh lighting-readability package `artifacts/package-lighting-readability-20260804/NightglassSignalDemo.app` built successfully; executable SHA-256 is `3c5e2708b191e7263bf9993fd6a720bfa539d19cddd3ba036be7ccd405372d41`. The post-fix Development rebuild also succeeded and produced `artifacts/package-mac-remote-input-fix-final-20260804/NightglassSignalDemo.app` with executable SHA-256 `4f93db747f045567615017ffbce81d881cc053f1bf7aa91c43c5bb0939c8a8e1`; local deep strict codesign and `validate_nightglass.py` pass. UE5.1/Win64, Windows server/client packaging, and release capture remain open. |
| Remote Mac Pro | Exact package and player-operated combat compatibility pass, visual/release presentation open | The remote lighting-readability package at `NightglassSignal-remote-20260804-lighting-readability-delta/NightglassSignalDemo.app` has matching executable and five cooked-container hashes, and deep strict codesign verification passes. A foreground run through `RemoteLightingPlay5.log` returned bridge `READY`, `PONG`, and live `STATE`; W moved the pawn from `x=0.00 y=-950.00` to `x=1914.38 y=1565.87` and then `x=3365.94 y=1565.97`. Accepted bridge inputs covered look, left-mouse fire, R reload, and G frag; the runtime recorded a server-spawned/player-requested frag, AI rifleman/assault/breacher grenade threats, AI movement, rifle fire, reload, and hit reactions, and a real player death at health 0.0. The post-fix executable was SHA-verified before remote re-signing and ran in an isolated clone on ports 1992/1993. The remote bridge released held W after an abrupt client disconnect, accepted a replacement client at speed `0.00`, and delivered LOOK, fire, and reload commands; evidence is `artifacts/remote-input-fix-20260804/`. Escape was accepted but did not transition the auto-start session to `paused=yes`, so paused-screen recovery remains open. Remote screen capture returned wallpaper only, and occasional non-zero AI body-to-movement yaw samples remain, so player-visible window proof, facing polish, UE5.1/Win64, and release gates remain open. Evidence: `artifacts/remote-lighting-readability-20260804/README.md`, `artifacts/remote-input-fix-20260804/`, `artifacts/local-remote-input-regression-20260804/`, and the prior role/map/reload/boundary evidence in `artifacts/remote-ai-facing-20260804/`, `artifacts/remote-exact-package-20260804/`, and `artifacts/remote-exact-playtest-20260804/`. |

## Latest remote recheck: 2026-08-05

The exact package in artifacts/package-opening-survival-state-20260805/ was
copied to the isolated Mac Pro path
NightglassSignal-remote-20260805-opening-survival-state/. The executable hash
is 0927f618f167ff32910bbd40e4c3868bfbf5ce669de1b29c40699f700e53f232;
global.ucas was also hash-verified, and deep strict codesign passed after
remote ad-hoc signing. PID 92915 accepted a held bridge session on port 1987.
The run proved player movement, enemy hand-mounted weapons, changing enemy
mount positions, enemy fire/reload/hit animation events, player ammo depletion,
and reload completion. It still ended with player health 0 while two hostiles
remained, so player-operated survivability remains an open requirement.
Full evidence is in artifacts/remote-opening-survival-state-20260805/.

## Latest critical-recovery recheck: 2026-08-05

The rebuilt staged Mac app has executable SHA-256
`94846ab6a4751d1475071c0fdb10c27b13c965812eb7a7e03222ea73c83c64bd` and was
archived for transfer with SHA-256
`8de5968267450bc2135066e5cb4cf37829f966e0ef6beb1169a6cf6aea3d1c20`. The
remote archive hash matched and deep strict code-signature verification passed
after extraction on the Mac Pro. A rendered attempt reached Glasswake but
hit Metal out-of-memory because three pre-existing rendered Unreal processes
were left running. The same exact package then ran with `-nullrhi` and a
persistent loopback bridge on port `3002`.

The packaged run reached the five-hostile checkpoint, accepted ordinary
movement/fire/reload input, depleted `30 -> 0` rounds, restored `30/180`, and
recorded `player critical recovery started health=28.1 duration=1.25 damage=4.0`.
State snapshots recovered into the mid-30s while hostile fire remained active.
This closes the source/package critical-recovery regression but remains
development bridge/null-renderer evidence; OS-keyboard/player-operated visual
survival, UE5.1/Win64, and release gates remain open. Full evidence is in
`artifacts/remote-critical-recovery-20260805/`.

## Latest damage-window-cap recheck: 2026-08-05

The follow-up exact staged package is
`artifacts/package-player-survival-20260805-tuned/`, with executable SHA-256
`e2c0d29be6cd3cbf23eae85ab088538d3254a389887cb637dc173954adcd180e`. The
matching remote package is
`/Users/muhammadluay/NightglassSignalRemote/package-player-survival-20260805-cap6/NightglassSignalDemo.app`;
all six executable/cooked-container hashes matched and deep strict codesign
verification passed.

The Mac Pro null-renderer run used `-BM_InteractionProbe` and the loopback
bridge on port `3013`. It reached phase 6/OpenCombat with eight hostiles.
Stationary health observations were `79.4 -> 42.9 -> 21.5 -> 28.9`, with no
elimination across roughly 130 simulated combat seconds. The ordinary reload
path restored `0/210` to `30/180`. This supports the reduced
`HostileDamageWindowCap=6.0f` contract, but it is not player-operated or
rendered survival proof. Evidence, launch flags, hashes, and limitations are
in `artifacts/remote-player-survival-cap6-20260805/README.md`.

## Current completion boundary

The project is a functional, source-first Unreal vertical slice with strong
UE5.8.1 Mac compatibility evidence. It is not yet the requested release-ready
UE5.1/Win64 AAA vertical slice because the player-operated mission capture,
production art/audio/cinematics, controller/accessibility visual review,
Windows build/performance proof, and multiplayer release soak are still open.
