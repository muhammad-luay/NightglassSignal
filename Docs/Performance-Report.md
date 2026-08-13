# Performance report

## Target

Windows PC, 1920x1080, scalable presets, 60 FPS target on a named mid-range
gaming computer. This is a target, not a result.

## Current measured state

The earlier Mac startup probe used UE5.8.1 Development targets with `-nullrhi`,
so that run has no renderer FPS or frame-time measurement. It did establish
that the runtime greybox can start on an Apple M1 Pro with 18 environment
pieces, 8 mission interactables, and both runtime directors.

A rendered packaged-client probe is now available. The first measurement was
contaminated by two older Nightglass processes, but a clean rerun is recorded
separately below. Neither Mac run is a substitute for the required isolated
Windows/mid-range-PC result.

| Hardware / engine | Build | Mode/map | Result | Capture |
| --- | --- | --- | --- | --- |
| Apple M1 Pro / 32 GB, macOS 26.3 / UE5.8.1 | Development Editor `-game`, `-nullrhi` | Standalone Entry / Glasswake greybox | startup and clean shutdown passed; no FPS claim | `artifacts/runtime-ue58/Runtime-run.log` |
| Apple M1 Pro / 32 GB, macOS 26.3 / UE5.8.1 | Packaged Development client, rendered | Glasswake opening mission, requested 1920x1080 / actual 1440x1080 | 1 probe plus 2 pre-existing Nightglass processes; avg 23.21 FPS, P95 51.49 ms / 19.42 FPS, max 53.00 ms, 60 FPS P95: **no** | `artifacts/performance-probe-view-validation-20260802-rerun/PerformanceProbe.log` |
| Apple M1 Pro / 32 GB, macOS 26.3 / UE5.8.1 | Packaged Development client, rendered, isolated | Glasswake opening mission, actual 1440x1080 | avg 72.15 FPS, P95 14.33 ms / 69.78 FPS, max 158.87 ms, 60 FPS P95: **yes** | `artifacts/performance-probe-clean-20260803/PerformanceProbe.log` |

UE5.1, an isolated rendered 1080p capture, Windows packaging, and
dedicated-server performance remain open.

The accelerated mission probe is a runtime progression check, not a frame-rate
measurement. On the same UE5.8.1 Mac development runtime it reached `Complete`
at 615.25 simulated seconds (10.25 minutes) with combat actors disabled and a
50x development time scale. Evidence:
`artifacts/mission-probe/GlasswakeProbe-20260731-214640.log`.

The project now includes `Scripts/run_performance_probe.sh`. It launches the
rendered Mac game at 1920x1080, starts the mission, records every local player
frame for a bounded interval, and logs average FPS, P95 frame time, P95 FPS,
maximum frame time, and whether the P95 sample fits the 60 FPS frame budget.
This is a repeatable measurement harness, not a substitute for the required
Windows mid-range-PC result. `-NoSound` may be removed when measuring audio
overhead. The current run’s full stdout and copied log are both preserved in
the artifact directory above.

## Required capture table

| Hardware / driver | UE version | Build | Mode/map | Players | Avg FPS | P95 frame ms | Game thread ms | Render thread ms | Server CPU | Client bandwidth | Capture |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| record | 5.1.x | Development/Shipping | Glasswake / Signal Control | record | record | record | record | record | record | record | attach |

## Procedure

1. Record commit, engine version, OS, CPU/GPU/RAM, driver, scalability preset,
   resolution, build configuration, server tick, and player count.
2. Warm up for one minute, then capture a fixed 120-second window in Glasswake
   and a fixed 120-second Signal Control window.
3. Record `stat unit`, `stat game`, `stat gpu`, `stat memory`, `stat net`, and
   Unreal Insights traces with `trace.start default,net,frame,render`.
4. Repeat with scalability Low/Medium/High and 2, 6, and 12 players.
5. Repeat Signal Control with `Net PktLag=100` and `Net PktLoss=5`.
6. Attach captures and update this table; do not substitute the 60 FPS target.
