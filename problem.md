# NightglassSignal level playtest problems

Date: 2026-08-11

Build tested: the freshly staged and archived Mac package at
`artifacts/package-todos-fixed-20260811/NightglassSignalDemo.app`.

Test method: each user-facing level 1–20 was launched independently from the
archived package with `BM_FullCampaignProbe`. The probe used real
CharacterMovement input, the normal player weapon/fire/reload and damage path,
real enemies with attached weapons, physical objective traces and hold
interactions, extraction-boat interaction, and final defense. It required the
exact `traversal=complete combat=complete objectives=relay+console
extraction=boat final_defense=complete phase=Complete` marker. Fresh evidence is
in `artifacts/playtest-full-campaign-todos-fixed-20260811/`.

Result: 20/20 levels passed the real full-campaign probe. The run recorded 848
enemy weapon attachments, 2,369 damage-confirmed player shots, 120 measured
traversal segments, and 60 physical objective/boat hold starts. A separate
packaged layout probe loaded all 20 distinct structural layouts and collision
cover sets; the enemy probe loaded all 20 encounter layouts and spawned real
campaign enemies. All 20 full-campaign logs reached Recast ready state. The
package passed `Scripts/verify_staged_maps.sh`.

## P-001 — StormVault extraction approach rejects a valid inside-radius start

- Severity: Blocker
- Status: Resolved and regression-tested
- User-facing level: 10
- Campaign index: 9
- Evidence: `artifacts/playtest-full-campaign-todos-fixed-20260811/9-StormVault.stdout.log`
- The traversal helper now accepts an approach that starts inside the 260 cm
  interaction radius while still requiring the real boat hold, final defense,
  and `phase=Complete`. StormVault passed the full probe.

### TODO

- [x] Preserve the real CharacterMovement/interaction check.
- [x] Require the boat hold, final defense, and `phase=Complete`.
- [x] Rebuild the staged package and rerun all 20 levels.

## P-002 — Navigation data is rebuilt at runtime on every level

- Severity: High
- Status: Resolved; runtime generation retained by measured performance
- Evidence: every log in `artifacts/playtest-full-campaign-todos-fixed-20260811/`
  reports `navigation: ready ... active_tiles=28 runtime_generation=dynamic`
  before its enemy wave is released. All 20 full-campaign runs completed. The
  fresh runtime build times were 22.21–119.76 ms, averaging 34.94 ms.
- Runtime generation is intentional because the physical battlefield is
  generated per campaign level. The old failure mode—combat starting before
  Recast was usable—no longer occurs in the verified package. With a 34.94 ms
  average build and 119.76 ms worst case across the 20 layouts, retain dynamic
  generation for now; revisit baked per-layout data only if shipping profiling
  shows a meaningful startup budget problem.

### TODO

- [x] Configure runtime generation for the generated collision layout.
- [x] Gate combat-wave release on navigation readiness.
- [x] Rerun all 20 levels and verify the ready-state path.
- [x] Measure startup cost and decide whether baked per-layout navigation is
  worthwhile for a production shipping build.

## P-003 — Enemy movement falls back from crowd navigation on every level

- Severity: High
- Status: Resolved and regression-tested
- Evidence: the fresh 20-level full-campaign run contains 7,212
  `navigation path active` events and zero `direct fallback active` events.
  All 20 levels passed with real CharacterMovement and combat movement.
- The controller now holds while a navigation system exists but Recast is not
  ready; direct CharacterMovement fallback is limited to genuinely nav-less
  sessions. There were zero `UCrowdManager`/Recast startup warnings in the
  fresh package.

### TODO

- [x] Ensure combat encounters wait for a usable Recast path before spawning.
- [x] Add movement displacement and forward-facing checks to the combat role
  probe.
- [x] Remove the transient direct-fallback window and the remaining crowd
  manager warning in later waves.

## P-004 — Runtime layout meshes are assigned while mobility is Static

- Severity: Medium
- Status: Resolved and absent from the final 20-level run
- Evidence: no `Calling SetStaticMesh ... but Mobility is Static` matches in
  `artifacts/playtest-full-campaign-todos-fixed-20260811/`.
- Runtime-generated walls, cover, and layout pieces retain valid collision in
  the final traversal/combat/extraction run.

### TODO

- [x] Set component mobility before assigning runtime meshes.
- [x] Verify collision-bearing walls and cover in all 20 full runs.
- [x] Rerun all 20 levels and require the mobility warning to be absent.

## Expected initialization noise not registered as gameplay problems

- Missing first-run profile files are expected because every level used a new
  playtest profile slot.
- LinuxArm64 device-profile messages are irrelevant to this Mac run.
- `Session CrashGUID` is a normal Unreal session identifier, not evidence of a
  crash.
- No `Unable to find RecastNavMesh instance while trying to create
  UCrowdManager instance` warning appeared in the fresh package run.
