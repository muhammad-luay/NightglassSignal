# Setup

## Requirements

- Unreal Engine **5.1.x** with the C++ desktop toolchain.
- Windows PC for the target runtime; a UE5.1 editor can be used on a supported
  development host.
- For the current Mac compatibility probe, Unreal Engine **5.8.1** is
  installed at `/Users/Shared/Epic Games/UE_5.8`.
- Git LFS if authored binary `.uasset`/`.umap` files are later added.

The repository does not include Unreal Engine, generated binaries, or platform
credentials. The project is still associated with UE5.1; use the installed
UE5.8.1 only as an explicit compatibility probe until the intended UE5.1
toolchain is available.

## Open the project

1. Open `NightglassSignal/NightglassSignal.uproject` in UE5.1.
2. Allow project files to regenerate.
3. Build `NightglassSignalDemoEditor` in Development Editor.
4. Open the default Entry map. `ABMGameMode` creates the greybox in `BeginPlay`.
5. Press Play, press Enter at the Black Meridian menu, and follow the first
   objective.

The generated scene intentionally uses `/Engine/BasicShapes/Cube` and engine
lighting. It does not need authored content to exercise the first playable
milestone.

On the current Mac compatibility probe, use the editor's game launch path so
the installed engine runtime libraries are available:

```text
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  NightglassSignal.uproject -game -windowed -ResX=1440 -ResY=900
```

The standalone target under `Binaries/Mac` is a development build, not a
staged/package deliverable; direct execution is not a release smoke test.

To produce a Mac compatibility package with the current UE5.8.1 distribution,
run the `BuildCookRun` command in [Build and packaging](Build-and-Packaging.md),
then copy and validate the staged application:

```text
Scripts/archive_staged_mac.sh artifacts/package-mac
```

Launch the resulting app from its `Contents/MacOS` directory for a local smoke
test. The intended release remains UE5.1/Win64.

## Run the local UE5.8 compatibility probe

From `NightglassSignal/` on this Mac:

```text
export UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
UE_ROOT="$UE_ROOT" REPORT_DIR="$PWD/artifacts/automation-local" Scripts/run_automation.sh
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" NightglassSignalDemoEditor Mac Development \
  -Project="$PWD/NightglassSignal.uproject" -WaitMutex -NoHotReloadFromIDE
```

The probe uses `-nullrhi` for deterministic headless checks. It validates the
Mac editor/game compatibility boundary; it does not change the project's UE5.1
association or certify the Windows release path.

## Run the accelerated mission pacing probe

After building the Mac editor target, the development-only probe starts the real
`ABMMissionDirector`, advances the ordinary phase clock at a controlled time
dilation, and records every phase transition. It does not replace a player
capture or certify authored cinematics, combat tuning, or the Windows build:

```text
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  TIME_SCALE=50 REPORT_DIR="$PWD/artifacts/mission-probe" \
  Scripts/run_mission_probe.sh
```

The command exits only after the runtime reaches `Complete`, or reports the
exact transition log if the probe exits or times out. `-BM_AutoStartMission`
is a command-line development probe; ordinary menu-driven sessions are
unchanged.

For the real player weapon fire-rate, accuracy-recovery, and first-person
procedural-sway contract on the current Mac compatibility engine, run:

```text
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  REPORT_DIR="$PWD/artifacts/combat-feel-probe" \
  Scripts/run_combat_feel_probe.sh
```

This development-only probe is numeric runtime evidence; it does not replace
the open player-visible animation/audio/VFX review.

To exercise the actual mission hold-interaction path as well as the phase
clock, run the development-only relay/console/extraction probe:

```text
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  TIME_SCALE=10 REPORT_DIR="$PWD/artifacts/interaction-probe" \
  Scripts/run_interaction_probe.sh
```

This moves only the probe pawn to the real relay door, optional archive
console, and extraction boat, then enters the same hold timer used by `E`.
It verifies authoritative interaction completion and mission completion; it is
not a substitute for UE5.1/Win64 replication or authored presentation QA.

To verify persistence across separate Unreal processes, run the isolated-slot
checkpoint/profile probe:

```text
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  REPORT_DIR="$PWD/artifacts/persistence-probe" \
  Scripts/run_persistence_probe.sh
```

The probe writes an isolated SaveGame slot from a deterministic Intelligence
checkpoint, starts a fresh process, loads the slot, validates transform,
health, loadout, active weapon, throwable inventory, optional mission state,
and profile unlocks, then calls the ordinary restart-from-checkpoint path. It
does not touch `BlackMeridian_Campaign` or `BlackMeridian_Profile`.

To verify that leaving the playable area cannot end the campaign on macOS, run
the packaged edge probe. This development-only probe drives the real
`CharacterMovementComponent` toward the perimeter and does not require host
keyboard automation:

```text
STAGED_APP="$PWD/Saved/StagedBuilds/Mac/NightglassSignalDemo.app" \
  REPORT_DIR="$PWD/artifacts/boundary-guard-edge-check" \
  Scripts/run_boundary_guard_probe.sh
```

The probe requires all four perimeter colliders, observes the player grounded
at the edge, and rejects `SQUAD LOST`, elimination, and legacy out-of-bounds
failure signals. It is a Mac/UE5.8.1 compatibility check, not a substitute
for the pinned UE5.1/Win64 release run.

To verify the enemy movement and weapon-presentation regression path in a
repeatable packaged session, run the combat-role probe:

```text
STAGED_APP="$PWD/Saved/StagedBuilds/Mac/NightglassSignalDemo.app" \
  REPORT_DIR="$PWD/artifacts/combat-role-probe" \
  Scripts/run_combat_role_probe.sh
```

It spawns the five-role showcase, requires all four role labels, requires
direct fallback movement events, and requires authoritative role-fire events.
It is development-only movement/weapon contract evidence, not a player-
operated survivability or UE5.1/Win64 release capture.

## Run the two-client combat/network probe

After building the Mac development editor target, the development-only combat
probe starts a listen host and two remote clients. It stages the two remote
players, calls the ordinary client fire path, verifies server-authoritative
ammo/damage plus replicated ammo/health, then terminates only the probe-owned
shooter client and checks the server bot takeover:

```text
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  STARTUP_SECONDS=60 MAX_SECONDS=70 REPORT_DIR="$PWD/artifacts/combat-probe" \
  Scripts/run_combat_probe.sh
```

This is UE5.8.1 Mac development evidence for one deterministic two-client
path. To exercise the bounded server rewind with Unreal packet emulation:

```text
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  PORT=7796 PKT_LAG=100 PKT_LOSS=5 \
  REPORT_DIR="$PWD/artifacts/combat-rewind-probe-lag-verified-20260802" \
  STARTUP_SECONDS=90 MAX_SECONDS=180 Scripts/run_combat_probe.sh
```

The verified run recorded `ping_ms=206.6`, `rewind_ms=103.3`, and `targets=6`.
This remains a bounded prototype check; it does not certify a 2–12-player
session, release-target packet-loss behavior, per-bone/anti-cheat validation,
or the pinned UE5.1 Windows release. The current probe does include the
implemented 25-degree player-view cone check for player-owned rays.

## Run the Signal Control host/client probe

After building the Mac development target, the development-only probe starts
one listen host and two local clients. It verifies two remote logins with
opposite team assignments, the authoritative countdown and live transition,
node rotation, and a real server-side score tick:

```text
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  PORT=7787 REPORT_DIR="$PWD/artifacts/signal-control-probe" \
  Scripts/run_signal_control_probe.sh
```

The script owns only the three processes it launches and refuses to use an
already-listening probe port. This is UE5.8.1 Mac runtime evidence; it does
not certify the pinned UE5.1 Windows release, dedicated-server packaging, or a
2–12 player production session.

To run this development probe with Unreal's packet emulation, add
`PKT_LAG=100 PKT_LOSS=5` to the command. That remains a narrow development
network check; it does not prove lag compensation or release readiness.

## Play Signal Control locally

From the campaign main menu, press `F4` to request a travel into the
Signal Control world. In a standalone session this uses `OpenLevel`; in a
listen/dedicated session the authority uses `ServerTravel`, so connected
clients follow the host. The mode opens its runtime lobby and the host presses
Enter to submit the server-authoritative five-second countdown.

Signal Control fills the 12-player ceiling after the human count is known,
using up to 11 balanced bots. Startup bots are placed at arena starts and
receive a visible weapon before the match becomes live.

For a direct provisioned-server smoke, the launch form remains:

```text
NightglassSignalDemo.exe /Engine/Maps/Entry?game=/Script/BlackMeridianRuntime.BMSignalControlGameMode -log
```

Use a provisioned local session rather than treating this command as a runtime
proof until it has been executed on the target engine build.
