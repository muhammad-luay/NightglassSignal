# Technical architecture

## Boundaries

```text
NightglassSignal.uproject
  NightglassSignalDemo              thin primary game module
  BlackMeridian plugin
    BlackMeridianRuntime             reusable C++ gameplay/runtime API
    BlackMeridianTests                UE Automation Framework tests
```

The legacy `MassCrowdSurvival` project in the parent repository is not a
dependency. The Black Meridian plugin is intentionally content-light: runtime
generated greybox geometry makes the first combat test possible while leaving
stable seams for authored maps, Blueprint assets, and final art.

## Ownership model

| Concern | Server authority | Client presentation |
| --- | --- | --- |
| Movement | CharacterMovementComponent simulation and replicated movement | local input, camera, interpolation |
| Fire validation | `ABMWeapon::ServerFire`, origin/rate/ammo checks, projectile spawn/detonation | input, recoil readback, spread recovery, and local sway |
| Hitscan rewind | `UBMCombatHistoryComponent` captures authority poses and `FBMCombatHistoryRewindScope` applies a bounded half-RTT rewind | none; the client supplies no rewind time or hit result |
| Damage and death | `UBMHealthComponent`, hit-zone/falloff calculation, bounded player critical-recovery window | hit marker, damage direction, VFX, low-health feedback |
| Weapon balance/presentation | serialized `UBMWeaponDataAsset` profiles / runtime fallback profiles | display name, ammo, mesh/socket references, optional audio/Niagara cues, and first-person presentation |
| Mission phases | `ABMMissionDirector` and `ABMGameState` | objective, subtitle, loading/results screens |
| Enemy decisions | `ABMEnemyAIController`, perception, squad memory | replicated enemy movement/presentation |
| Signal Control | `ABMSignalControlGameMode`, `ABMControlNode` | travel entry, lobby, countdown, scoreboard, and result screen |
| Settings | `UBMGameUserSettings` config persistence | FOV, sensitivity, input toggles, accessibility/audio values, optional Sound Mix/Class routing, and compact settings navigation |
| Character presentation | `UBMCharacterAnimationDataAsset`, `IBMCharacterAnimationInterface` | movement state, aim offset, montage events, optional first-person arms and third-person AnimBP |
| Surface response | `UBMSurfaceRuleDataAsset` plus physical-surface config | impact surface, penetration count, and authored rule lookup with greybox tag fallback |

Clients never submit damage, ammo, score, node control, match-state deltas,
rewind time, or hit results. The current prototype accepts a client aim
direction after a bounded origin check. For player-owned hitscan shots, the
authority derives one-way latency as half the replicated player-state ping,
clamps it to `UBMCombatHistoryComponent::MaxRewindSeconds` (250 ms), and
temporarily rewinds eligible combatants to interpolated authority samples while
the server trace runs. The history is authority-only, stance-aware, sampled at
50 ms, and bounded to 32 samples/one second; projectile shots are not rewound.
The authority also compares a player-owned shot direction to the replicated
controller view and rejects requests more than 25 degrees away. This blocks
the common around-cover/behind-self ray forgery while preserving a tolerance
for network quantization and movement. This is still a functional prototype
rather than full per-bone or anti-cheat telemetry validation. Unreal `CharacterMovementComponent` supplies the
baseline client prediction, replicated remote interpolation, and server
reconciliation; the player adds bounded server requests for
sprint/tactical sprint, aim, and prone so the authoritative movement-speed
state follows those predictions.

## Gameplay components

- `UBMHealthComponent` owns replicated health, regeneration policy, and death
  delegates. Hostile damage against the player has one bounded critical-health
  recovery episode per full-health cycle: the first lethal/critical hit leaves
  at least the configured minimum health, starts a short recovery pulse, and
  restores only up to the configured cap. Damage during the pulse remains
  authoritative and cannot postpone it; normal death remains possible after
  the episode is consumed.
- `UBMWeaponDataAsset` and `UBMThrowableDataAsset` are serialized designer
  profiles under `Content/Data/Weapons` and `Content/Data/Throwables`. Actors
  load those profiles by ID before falling back to source-only defaults; the
  fallback exists for an incomplete checkout, not as the normal balance path.
- `ABMWeapon` owns no balance constants. It composes
  `UBMFireModeComponent`, `UBMRecoilComponent`, and
  `UBMAmmunitionComponent`, consumes a `UBMWeaponDataAsset`, and exposes
  attachment-slot names for optics, muzzles, barrels, magazines, grips, and
  stocks. The fire-mode component tracks shot penalty and time-based recovery;
  the player applies the data-driven procedural sway locally. `ABMProjectile`
  is a replicated, server-detonated launcher path with owner filtering and
  radius falloff.
- `ABMCombatCharacter` provides team/health/elimination behavior to player,
  enemy, and friendly characters. It also publishes animation state, aim-offset,
  and named montage events through `IBMCharacterAnimationInterface`; no
  authored animation is required by the gameplay path.
- `UBMCombatHistoryComponent` is an authority-only, fixed-rate pose ring buffer
  attached to every combat character. `FBMCombatHistoryRewindScope` saves the
  current transform, stance, and collision state, applies available historical
  poses for one server trace, and restores each character even when the trace
  causes a death.
- `ABMPlayerCharacter` contains only input-to-component orchestration; mission,
  AI, and match state stay outside the character.
- `UBMSquadSubsystem` provides short-lived target memory to enemy controllers.
- `IBMInteractionInterface` separates doors, consoles, pickups, revives,
  switches, ladders, planting, breach, mounted sequences, and mission events
  from the player trace. The concrete actor replicates its interaction
  metadata, including a dynamic revive target, so clients can display and
  submit runtime-spawned prompts. The player owns the server-validated ladder
  and mounted-weapon movement transitions.
- `ABMThrowable` keeps frag damage and Veil suppression on the authority. Veil
  overlaps opposing combat characters and feeds the existing replicated
  suppression component, allowing the AI state machine to enter its
  `Suppressed` state without introducing a second crowd-control system.
- `UBMCharacterAnimationDataAsset`, `UBMAudioDataAsset`, and `UBMVFXDataAsset`
  are optional replacement contracts. Missing assets leave the runtime cube,
  runtime-authored UMG, and debug presentation active; configured assets are
  loaded only at
  presentation points and are never required by dedicated-server logic. Audio
  routes include indoor/outdoor and surface gunfire, surface-aware footsteps,
  reload/equipment, ambient/music/dialogue, low-health, and suppression cues.
- `ABMWeapon` resolves physical-material surface IDs into the shared surface
  enum, applies the selected data-asset rule, and carries surface metadata in
  `FBMDamageContext`. `ThinCover` remains an explicit fallback for generated
  greybox actors.

Hitscan shots allocate no projectile actors; launcher shots use the short-lived
`ABMProjectile` actor and throwables use their own short-lived actor path. A
pool would add state complexity without a measured benefit at this slice size;
authored Niagara impact/shell/projectile pools are the planned place to
introduce pooling once profiling shows allocation pressure.

## AI architecture

The source-only vertical slice uses `UAIPerceptionComponent` with sight and a
small explicit state machine: Patrol, Investigate, Engage, Flank, Suppressed,
and Retreat. Perception has a radius, peripheral angle, max age, and reaction
delay seam. `UBMSquadSubsystem` shares a target for four seconds so an enemy
can react to communication rather than global omniscience. The next authored
AI pass can swap the decision layer for a Behavior Tree/State Tree/EQS asset
without changing character or damage APIs. Friendly squad members
independently select visible Vanta targets on the authority, move to a
line-of-sight-blocking thin-cover point, advance toward breach/arena/extraction
anchors, and can assist or revive squadmates while keeping formation.

## Runtime greybox

`ABMGreyboxDirector` builds the insertion yard, relay-house cover, seawall,
extraction pier, lighting, console, and spawn lanes from engine BasicShapes.
Geometry is local-generated on clients; enemies, friendlies, consoles, and
nodes are server actors where replication matters. Authored `.umap` content can
replace the director after route and combat validation.

## Network modes

- `ABMGameMode` is the single-player/mission default.
- `ABMSignalControlGameMode` disables mission-only actors, creates three
  `ABMControlNode` actors, rotates the active node, and scores server-side.
- `ABMPlayerController` requests authoritative `ServerTravel` for listen or
  dedicated sessions and uses `OpenLevel` for standalone iteration. Signal
  Control presents a runtime lobby until the host submits `RequestMatchStart`,
  then replicates a five-second countdown before scoring.
- Signal Control spawn selection scores candidate starts by recent combat age,
  opposing-character distance, and visibility traces. Bots use the same safe
  start selector; this is a deterministic prototype heuristic, not a final
  nav/EQS spawn system.
- Signal Control maintains a 12-slot ceiling dynamically: humans consume slots
  first and the server fills the remainder with up to 11 balanced bots. Bot
  fill waits for runtime start points, and each bot receives a profile weapon
  with third-person presentation before the match goes live.
- `ABMMissionDirector` writes checkpoint SaveGame data for phase, optional
  intel, jammer choice, health, transform, current weapon, loadout, and
  throwable inventory. Restarting a loaded checkpoint asks
  `ABMGreyboxDirector` to destroy stale hostile waves, restore friendlies and
  mission props, clear combat counters, and multicast the breach-environment
  baseline before the saved phase resumes. Runtime mission actors consult
  replicated mission phase before accepting console, breach, jammer, or
  extraction interactions.
  `ABMGameMode` loads a basic profile, selects starter loadout IDs, records
  total multiplayer/campaign completions, and persists the set of completed
  campaign variants independently from the checkpoint slot.
- Target classes exist for game, editor, and dedicated server builds.
- Online Subsystem Null is enabled for local listen-server iteration. No
  external matchmaking, relay, or platform credentials are required.

## Directory structure

```text
Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public|Private
  BMCoreTypes, BMWeapon*, BMHealthComponent
  BMCharacterAnimationDataAsset, BMCharacterAnimationInterface
  BMCombatCharacter, BMPlayerCharacter, BMEnemyCharacter, BMFriendlySquadMember
  BMEnemyAIController, BMSquadSubsystem
  BMGameMode, BMGameState, BMPlayerState, BMPlayerController, BMHUD
  BMMissionDirector, BMGreyboxDirector, BMInteractableActor
  BMControlNode, BMSignalControlGameMode
Plugins/BlackMeridian/Source/BlackMeridianTests
Source/NightglassSignalDemo
Content/Characters, Weapons, Animations, AI, Levels, UI, Audio, VFX, Data
Config, Docs, Scripts
```

## Naming and implementation rules

`ABM` is an actor, `UBM` is a UObject/component/data asset, `FBM` is a
struct, and `EBM` is an enum. C++ owns authority, reusable state, validation,
and replication. Blueprints own authored presentation and lightweight
orchestration. Tuning values belong in data assets/config, not weapon or
character methods. Every deferred system is named in [Known Issues](Known-Issues.md).

## Greatest technical risks and mitigations

1. **Engine/API drift:** pin the project and build scripts to UE5.1, keep
   engine-dependent checks in the target/automation gates, and isolate gameplay
   behavior in the plugin.
2. **Network authority and cheating:** validate server ammo, rate, origin,
   friendly fire, damage, score, and node state; keep the bounded rewind
   server-owned and extend the current 25-degree view-cone guard with
   per-bone and anti-cheat telemetry validation before a public competitive
   release.
3. **Scope and content cost:** prove combat with runtime greybox and replaceable
   data/assets first; defer final art/audio/cinematics until the route is fun.
4. **AI readability/performance:** cap perception, use short-lived squad memory,
   role profiles, and measured cover/nav queries; profile before adding agents.
5. **Unmeasured performance:** keep the target separate from results, capture
   named hardware/scalability/frame-time evidence, and use scalable presets and
   pooled/reused effects where persistent effects justify the complexity.
