# Build and packaging

## Editor build

```text
<UE_ROOT>/Engine/Build/BatchFiles/RunUAT.bat BuildCookRun \
  -project="<repo>\\NightglassSignal\\NightglassSignal.uproject" \
  -noP4 -build -cook -stage -pak -archive -platform=Win64 \
  -clientconfig=Development -serverconfig=Development \
  -archiveDirectory="<repo>\\artifacts\\black-meridian"
```

The repository-owned `Scripts/build_unreal.sh` is the Linux/CI equivalent and
expects `UE_ROOT`; `Scripts/run_automation.sh` also detects a Mac host and
uses the installed app-bundle executable at
`Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor`. The Windows
target is the intended release platform; do not claim a package until the
output contains both a client and a server target and the packaged smoke test
has been recorded.

The current Mac probe built the editor and game targets with UE5.8.1. The
installed engine distribution reports that dedicated-server targets are not
supported, so this Mac result is not a server/package substitute.

## Windows client/server package

The release target remains UE5.1/Win64. From a Windows development machine
with the UE5.1 desktop toolchain installed, run the repository-owned wrapper
from a `cmd.exe` prompt:

```bat
set UE_ROOT=C:\Program Files\Epic Games\UE_5.1
set BUILD_SERVER=1
Scripts\build_windows.bat
```

The wrapper runs `BuildCookRun` with client and dedicated-server targets and
archives to `artifacts\package-win64` by default. A client-only iteration can
set `BUILD_SERVER=0`, but it does not satisfy the release server gate. Verify
the resulting archive before calling it a packaged build:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\verify_windows_package.ps1 `
  -PackageRoot artifacts\package-win64 `
  -RequireServer
```

The verifier requires the client executable, a cooked project content
container under `Content\Paks`, and (with `-RequireServer`) a separate server
executable. It launches the packaged client with `-nullrhi -ExecCmds=Quit`,
then starts the server long enough to prove it remains alive before stopping
only that verifier-owned process. The verifier does not replace the required
1080p/60 hardware measurement, two-to-twelve-player soak, or player-operated
mission capture.

On the current UE5.8.1 Mac distribution, `BuildCookRun -archive` copied the
small `Binaries/Mac/NightglassSignalDemo.app` wrapper without the staged
`Contents/UE` payload. After the cook/stage command completes, copy the
verified staged application into the archive directory with:

```text
Scripts/archive_staged_mac.sh artifacts/package-mac
```

The script checks that the executable and `NightglassSignal-Mac.utoc` are
present before reporting an archive. The latest verified compatibility artifact
is `artifacts/package-mac-audio-20260804/NightglassSignalDemo.app`;
its null-renderer smoke loaded both authored map shells and its packaged reload
probe completed at the shortened timers. This remains Mac/UE5.8.1 evidence, not
the intended Win64/UE5.1 release package.

For the saved-map compatibility gate, run the staged-map smoke after staging:

```text
REPORT_DIR="$PWD/artifacts/staged-map-smoke" \
  Scripts/verify_staged_maps.sh
```

It launches the staged app twice and requires `LoadMap` completion for both
`/Game/Levels/Glasswake` and `/Game/Levels/SignalControl`. This is a Mac/UE5.8.1
cooked-map check, not Windows/UE5.1 release proof.

## Automation

```text
NightglassSignal/Scripts/validate_nightglass.py
NightglassSignal/Scripts/run_automation.sh
NightglassSignal/Scripts/run_signal_control_probe.sh
NightglassSignal/Scripts/run_combat_probe.sh
NightglassSignal/Scripts/run_signal_results_probe.sh
NightglassSignal/Scripts/run_persistence_probe.sh
NightglassSignal/Scripts/run_interaction_probe.sh
NightglassSignal/Scripts/run_interaction_acceptance_probe.sh
NightglassSignal/Scripts/run_packaged_reload_input_probe.sh
NightglassSignal/Scripts/run_combat_role_probe.sh
NightglassSignal/Scripts/run_combat_feel_probe.sh
```

Run the Python validator on any machine. Run the Unreal automation script on a
runner with the intended UE5.1 installed for release validation. On this Mac it
can be run with `UE_ROOT` pointing to UE5.8.1; it executes the
`BlackMeridian` automation suite with `-nullrhi`.

## Server/client smoke

1. Launch the packaged server with `-log -port=7777`.
2. Launch two development clients with `127.0.0.1:7777`.
3. Capture team assignment, movement, fire-rate/ammo ownership, node rotation,
   score parity, a disconnect, and the results screen.
4. Repeat with `Net PktLag=100` and `Net PktLoss=5` in a development build.

The first smoke run must record engine version, commit, map, mode, build type,
client count, and any unresolved defects.

The repository also includes `Scripts/run_signal_control_probe.sh` for a
development-only UE5.8.1 Mac listen-server check. It is intentionally smaller
than the release smoke: it uses a deterministic authoritative scoring bot and
does not replace packaged-server, packet-loss, or 2–12 player validation.

`Scripts/run_combat_probe.sh` is the matching development-only two-client
combat check. It records the client fire request, authoritative server ammo and
damage, replicated ammo/health, and a probe-owned disconnect with bot takeover.
With `PKT_LAG=100 PKT_LOSS=5`, it also exercises the bounded server-side
half-RTT hitscan rewind. The verified artifact is
`artifacts/combat-rewind-probe-lag-verified-20260802/`; this remains a deterministic
UE5.8.1 Mac check and does not replace release-target packet-loss,
per-bone/anti-cheat validation, or 2–12-player soak QA. Player-owned rays do
exercise the current 25-degree controller-view cone guard.

`Scripts/run_persistence_probe.sh` launches two separate UE5.8.1 Mac
development processes and validates isolated checkpoint/profile serialization
plus restart-from-checkpoint restoration. It does not certify UE5.1/Win64 save
migration or replicated release behavior.

`Scripts/run_signal_results_probe.sh` launches the Signal Control game mode
directly, shortens only the development probe score limit, and verifies that
the authoritative winner and both team scores reach the results HUD. The
2026-08-01 run recorded Lattice as the winner at 2–0 in
`artifacts/signal-results-probe/`; this is presentation/state evidence, not a
packaged release result.

`Scripts/run_packaged_reload_input_probe.sh` launches the staged Mac executable
and scans its stdout for the real `R` input path, rather than relying on an
editor-only `-abslog` file. The latest package passes the ARX-4 partial reload
at `0.45s` and empty reload at `0.65s` in
`artifacts/package-mac-audio-20260804/frozen-reload-probe/`. The same staged
package also passes the combat-role and non-failing-boundary probes in the
adjacent `frozen-combat-role-probe/` and `frozen-boundary-probe/` directories.

Both development probes accept `PKT_LAG` and `PKT_LOSS`, which are forwarded to
Unreal as `-PktLag` and `-PktLoss`. For example:

```text
PKT_LAG=100 PKT_LOSS=5 \
  UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  REPORT_DIR="$PWD/artifacts/signal-control-probe-packet-loss" \
  Scripts/run_signal_control_probe.sh
```

This records packet emulation only; it is not lag-compensation or 2–12-player
release evidence.
