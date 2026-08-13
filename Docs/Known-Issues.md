# Known issues

Last reviewed: 2026-08-13.

## Latest package validation snapshot

The final UE5.8.1 Mac cinematic package is locally staged at
`artifacts/package-cinematic-pass-20260804/NightglassSignalDemo.app` with
executable SHA-256
`30fa641aa5ff6b6539fca0558f63dd5a4e583d58fbb3e735dc2946cb4817da64`.
Source validation passed for 79 runtime files and the editor automation report
is 33/33 succeeded with zero warnings or failures. The local package passed
combat-role, reload, boundary, and accelerated cinematic mission probes. The
same package was checksum-verified and exercised on the Mac Pro in the isolated
`NightglassSignal-remote-20260804-cinematic-final-seeded` directory; remote
evidence is in `artifacts/remote-cinematic-final-20260804/`.

These are development/compatibility probes. They do not close the remaining
player-operated 10–15 minute capture, authored Level Sequences/production
assets, full-screen permission, UE5.1/Win64, Windows performance, or
2–12-player release-soak gates.

1. **UE5.1 and Win64 release validation are unavailable on this workstation.**
   UE5.8.1 is installed and has passed the Mac editor/game compile, 33-test
   automation suite, cooked/staged client build, and a packaged null-renderer
   Entry-map greybox smoke. The project is
   still pinned to UE5.1, and the installed UE5.8 distribution reports that
   dedicated-server targets are unsupported. UE5.1 compatibility, Win64 SDK,
   Windows packaging, dedicated-server packaging, and two-client release
   behavior remain unverified. The unbundled `Binaries/Mac/NightglassSignalDemo`
   target is not a packaged application and should be launched through the UE
   editor `-game` path (or a staged package); direct execution does not include
   the engine runtime libraries.
2. **Runtime greybox is not authored art.** It uses engine BasicShapes, debug
   tracer lines, placeholder materials, and runtime-authored UMG text. The
   legacy canvas path remains a compatibility fallback. Replace the greybox
   presentation only after combat routes are validated.
3. **Mission cinematics are represented by playable state changes and
   subtitles.** Camera tracks, voice performances, destruction simulation, and
   final audio are not complete assets. A development-only accelerated pacing
   probe now verifies the ordinary phase clock reaches Complete, but that is
   not a player-captured 10–15 minute presentation pass.
4. **Weapon RPC validation is prototype-grade.** Origin distance, a 25-degree
   player-view alignment check, server fire
   interval/ammo, and projectile ownership/detonation are checked; a
   UE5.8.1 Mac development combat probe proves a remote-client hitscan request,
   server-authoritative damage, replicated ammo/health, and a probe-owned
   disconnect. The bounded prototype now keeps one second of authority-only
   stance-aware pose history and rewinds player-owned hitscan traces by half the
   reported RTT, capped at 250 ms. The packet-emulated run at `PktLag=100` and
   `PktLoss=5` recorded `ping_ms=206.6`, `rewind_ms=103.3`, and `targets=6` in
   `artifacts/combat-rewind-probe-lag-verified-20260802/CombatHost-20260802-144452.log`.
   Anti-cheat telemetry, per-bone rewind, reload synchronization,
   release-target packet-loss behavior, and authored
   suppressed cues remain open.
5. **Reload and first-person presentation need engine QA.** Reload interruption
  now has a server timer/cancel path and source automation coverage. Character
  animation state/events, optional first-person arms, montage routing, shell
  ejection, surface-impact/decal/blood gating, detonation cues, suppression
  post-process, surface-aware footsteps, low-health/ambient/music/dialogue
  routing, and optional audio/Niagara weapon presentation are wired. The
   fresh UE5.8.1 Mac rendered combat showcase verifies fallback enemy
   locomotion, role weapon selection, a readable third-person `hand_r` mount,
   enemy fire from the authored weapon muzzle, and the player-held first-person
   weapon. All six firearm profiles have original low-poly project-owned
   placeholder meshes with `Grip`/`Muzzle` sockets, and the rebuilt
   30-test automation suite passes with zero warnings or failures in
   `artifacts/automation-combat-audio-20260804-clean/`. Authored montages,
   cues, production art, and final animation synchronization are still not
   claimed until UE5.1 runtime
   evidence exists. A new development-only combat-feel probe now verifies the
   ordinary player fire interval (`0.0923s` ARX-4), immediate/half-interval
   rejection, post-interval acceptance, accuracy recovery (`0.350` back to
   `0.000`), and numeric first-person weapon-root movement; this does not close
   the rendered/player-visible sway, authored animation, audio/VFX, or
   UE5.1/Win64 gates. Local evidence is in
   `artifacts/combat-feel-probe-packaged-local/`. The isolated Mac Pro package
   also passed the same probe on 2026-08-04: `30->29` first shot,
   `29->29` immediate and half-interval rejection, `29->28` post-interval
   acceptance, accuracy `0.000->0.350->0.000`, and sway deltas of `0.339 cm`
   and `0.351 deg` in
   `artifacts/remote-fresh-probes-20260804/CombatFeelFresh.stdout.log`.
   This remains a development/null-renderer contract, not authored
   animation/audio/VFX or UE5.1/Win64 release proof.
6. **Signal Control is a code path, not a certified 2–12 player release.**
   Main-menu travel into the mode, replicated runtime lobby roster (host/team/
   bot/capacity), server start request, countdown, bot fill, respawn delay,
   recent-combat/visibility-aware spawn scoring, kill-feed state, latency data,
   and same-team bot takeover on live disconnect are now represented. A UE5.8.1
   Mac development listen-server
   probe verifies two remote logins, opposite team assignment, match-live state,
   node rotation, and an authoritative score tick. A separate UE5.8.1 Mac
   development combat probe verifies a two-remote-client fire/damage path and
   one probe-owned disconnect with server bot takeover. Authored lobby UI,
   broader 2–12-player disconnect recovery, release-target packet-loss
   behavior, and release-target runtime proof remain open. The separate
   `PktLag=100`/`PktLoss=5` development check is recorded above. A focused
   results probe now confirms the winner and both scores reach the HUD, but it
   uses a shortened development-only score limit and is not release QA.
   The current lobby uses the runtime-authored `UBMNativeUIWidget`; authored
   Widget Blueprint styling and a player-captured lobby review remain open. The final
   rebuilt-module run added sixteen perimeter starts so a joining
   client has a free slot before bot trimming; `artifacts/signal-control-final-20260802/`
   verifies two remote logins, balanced 12-slot fill, live scoring/rotation,
   and zero spawn-collision or live-pawn-recovery warnings.
7. **Checkpoint and profile persistence are verified only on the current
   compatibility runtime.** A two-process UE5.8.1 Mac development probe now
   writes an isolated checkpoint/profile slot, starts a fresh Unreal process,
   restores phase, transform, health, loadout, active weapon, throwable
   inventory, optional state, and profile unlocks, then exercises the real
   restart-from-checkpoint path. UE5.1/Win64 serialization, replicated
   restart capture, and release-save migration remain open.
8. **Interaction actions still need authored presentation and full network
   QA.** Tap/hold, doors, ladders, weapon/ammo pickup hooks, switches, planting,
   breach, revive prompts/restoration, mounted-weapon state, and mission-event
   IDs are represented. A UE5.8.1 Mac development interaction probe now
   verifies the real relay, console, and extraction hold timers through mission
   completion. The rebuilt-module interaction acceptance probe also verifies
   runtime cover selection, mounted enter/exit, the replicated destruction
   event, and the ordinary revive hold/restore path at
   `artifacts/interaction-acceptance-probe-20260802/`. A separate rebuilt-module
   throwable probe verifies the serialized Frag and Veil assets, fuse timing,
   authoritative frag damage, and Veil suppression at
   `artifacts/throwable-probe-current-20260802d/`. Rendered first-person Frag
   and Veil presentation through the real throw path is captured at
   `artifacts/throwable-presentation-live-20260802/`. UE5.1/Win64
   client-server replication and authored interaction/cinematic assets remain
   open.
9. **Settings use a compact runtime-authored UMG implementation.** Keyboard/controller
   selection now edits FOV, independent sensitivities, input toggles,
   accessibility values, scalable graphics presets, audio mix values,
   friendly-fire policy, and a bounded set of persisted keyboard binding
   presets. Preset remaps reject reserved menu keys, atomically swap keys
   between remappable actions, and leave controller mappings unchanged;
   `BlackMeridian.Input.ConflictSafeRemapping` covers that transaction.
   Runtime front-end buttons now also expose D-pad selection, controller-A
   activation, and controller-B back behavior through the same action wrappers
   as mouse/keyboard input. This is source and UE5.8.1 automation coverage
   only; the isolated `32/32` automation result is in
   `artifacts/automation-controller-menu-20260804/`; a player-visible
   controller review, release-target mapping,
   arbitrary-key capture, and full authored UMG remapping remain open.
    Optional authored Sound Mix/Class references and weapon audio/VFX data
    profiles are applied when configured; generated WAV/SoundWave and Niagara
    placeholder assets are present for the compatibility build, while final
    authored cues remain open.

10. **Surface rules have a source contract, not authored materials.**
    `UBMSurfaceRuleDataAsset`, physical-surface config, and hit-context metadata
    now support Concrete/Metal/Wood/Glass/SoftCover resolution. The generated
    greybox still falls back to actor tags because no `.uasset` physical
    materials are present.

11. **Development cheats are intentionally narrow.** `BM_CheatGiveAmmo` and
    `BM_CheatRestore` are authority-checked and compiled as no-ops in shipping;
    there is no shipping console-cheat surface. They still need a development
    build smoke test on UE5.1.

12. **Playable camera cues are source scaffolding, not authored cinematics.**
    Intro, extraction, and ending cues replicate a small additive camera state
    and preserve look control by default. Level Sequences, animation tracks,
    voice performances, and destruction timing still require authored content
    and UE runtime review.

13. **Campaign operations are runtime-generated variants, not authored maps.**
    The selector now exposes twenty operations. Each changes the Entry-map
    route anchors, start, collision-bearing rooms/walls/platforms/cover,
    pickup, landmark dressing, and encounter budget/role cadence. The saved
    Glasswake shell is suppressed for campaign play; only the invisible safety
    floor and perimeter recovery colliders are shared. Separate production
    `.umap` layouts, production art, and UE5.1/Win64 level validation remain
    open; the UE5.8 Mac structural smoke currently covers levels
    1/2/3/4/9/13/14/16/18/20, with the full twenty-level package matrix still
    pending.

14. **Function-row keys can be consumed by macOS.** Bare `F2` is treated as a
    brightness key on some Mac keyboard settings and therefore never reaches
    Unreal. The packaged controller now registers direct `F2` and `K` paths and
    guards duplicate dispatch, but that cannot recover a key consumed before
    Unreal sees it. `Fn+F2` reaches Controls; `K` is also bound as a
    cross-platform fallback and is presented as the primary shortcut on the
    main menu/pause screen. Fresh packaged evidence is recorded at
    `artifacts/f2-robust-k-live-20260802.png` and
    `artifacts/f2-robust-fn-f2-window-20260802.png`.
15. **The packaged character-source mismatch is fixed in the current Mac
    package.** The project-local `SKM_Manny_Simple` is now a hard cook
    dependency, the project rifle clips run through `AnimationSingleNode`, and
    the MoverExamples plugin is no longer enabled or force-cooked. The fresh
    package records `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple` for
    all five showcase enemies, plus stable `hand_r` weapon mounts and role
    fire, in `artifacts/local-manny-showcase-20260803/`. Production soldier
    art, authored AnimBP/montages, and UE5.1/Win64 validation remain open.
16. **Enemy return fire was previously too sparse in the live encounter.** The
    current compatibility package now uses a bounded 120 ms AI decision tick,
    target-scoped three-round bursts for assault/rifleman roles, and
    weapon-rate-aware cooldowns. The fresh rendered package run records 19
    role-fire events and 19 corresponding combat-animation events across
    Assault (11), Breacher (2), Rifleman (4), and Marksman (2), with visible
    tracer activity in
    `artifacts/return-fire-burst-acceptance-20260803.png`. Production authored
    weapon/audio/VFX parity and UE5.1/Win64 validation remain separate open
    items.
17. **Player reload pacing was too slow in the older package and was still
    reported as too slow during playtest.** The current source and serialized
    profiles are now shortened; do not use the older reload packages for
    current playtests. The profiles use normal/empty timers of ARX-4
    `0.45s`/`0.65s`, Kestrel-9 `0.40s`/`0.55s`, Rook DMR `0.70s`/`0.90s`,
    Breach-12 `0.80s`/`1.05s`, Solace `0.35s`/`0.50s`, and Arc-7
    `0.85s`/`0.90s`; held fire no longer cancels an active reload every tick.
    The source two-client probe in
    `artifacts/reload-probe-pacing-source-final-20260803/` records the ARX-4
    `0.45s` timer, completion, replicated reload events, and ammo restoration.
    The rebuilt rendered showcase records about `0.455s` from
    `ReloadStarted` to `ReloadCompleted` in
    `artifacts/reload-visible-pacing-final-20260803/`. Empty input-to-ready,
    player-operated capture, and UE5.1/Win64 release behavior remain open until
    separately captured. A new UE5.8.1 Mac input-path probe now sends `R`
    through `PlayerController::InputKey`, records that the reload action binding
    fires, and measures ARX-4 partial/empty input-to-ready at about `0.50s`/`0.65s`;
    evidence: `artifacts/reload-input-probe-20260803/`. The rebuilt empty-magazine two-client probe now
    confirms the ordinary client reload request, `0/150 -> 30/120` restoration,
    replicated start/completion events, and the serialized `0.65s` empty timer
    in `artifacts/reload-probe-empty-followup-empty-20260803/`; this is not a
    substitute for the rendered/manual input capture. The fresh package that
    contains this pass is
    `artifacts/package-mac-menu-layout-20260803/NightglassSignalDemo.app`;
    its packaged `R` probe passes at `0.45s` partial and `0.65s` empty, with
    output in `artifacts/package-mac-menu-layout-20260803/reload-input-probe/`.
    The newer rebuilt package
    `artifacts/package-mac-reload-presentation-20260803b/NightglassSignalDemo.app`
    repeats the real packaged `R` path and the isolated rendered showcase
    reaches reload start/mid/complete in about `0.466s` at the weapon events
    (`0.503s` at the showcase poll boundary); the run log is in
    `artifacts/package-mac-reload-presentation-probe-20260803c/20260803-140442/`.
    The packaged screenshot requests still do not produce the three PNG files,
    and a player-operated capture plus UE5.1/Win64 release behavior remain
    open. Keep this issue active because the user still reports a long reload
    in play; verify that the current rebuilt package, not an older archive, is
    being tested.
    The fresh Mac Pro packaged input run now records the same path at
    `0.46s` partial and `0.65s` empty in
    `artifacts/remote-fresh-probes-20260804/ReloadInputFresh.stdout.log`;
    this narrows the remaining issue to player-operated visual confirmation
    and older-package avoidance. The fresh local goal package repeats the
    packaged path at `0.45s` partial and `0.65s` empty and reaches rendered
    reload start/mid/complete at `0.479s` in
    `artifacts/package-goal-reload-input-20260804/` and
    `artifacts/package-goal-reload-presentation-20260804/`; keep the issue
    open until the player-operated Mac Pro capture is made against this exact
    package.

18. **Opening hostile fire was too lethal during the latest playtest.** The
    previous combat package allowed a shared hostile shot every `0.16s`, kept
    the opening wave near-perfect at `1.8` degrees of aim error, and could
    remove the player's 100 health in roughly one second of exposed fire. The
    current source pass uses a `0.55s` squad fire budget, `0.28` opening damage
    (`0.22` for Breacher), a total `22` per-hostile-shot budget, a `38`
    hostile-explosion cap, and a shared `24` damage-per-second target budget
    across hostile weapons and ordnance, while adding role-specific `3.0`–`6.5`
    degree aim error. The `BlackMeridian.Combat.HostileDamagePacing` automation
    test now guards the contract. The fresh staged package at
    `artifacts/package-hostile-budget-20260803/` confirms five visible armed
    roles and a controlled combat run with an authoritative `3.6` damage hit,
    no player elimination, and no one-second death. A player-operated
    opening-wave survival capture is still required before this issue can be
    closed; later-wave pressure remains intentionally higher. The repeatable
    packaged combat-role contract also passes with five spawned enemies, four
    role labels, five movement events, and 60 authoritative role-fire events
    in `artifacts/combat-role-probe-followup-20260803/CombatRoleProbe.result.txt`.
    The exact fresh package at
    `/Users/muhammadluay/Desktop/NightglassSignal-remote-20260804-opening-pacing-fast/NightglassSignalDemo.app`
    was rerun on the Mac Pro. Its role probe recorded five enemies, four
    roles, five direct fallback movement activations, 67 role-fire events,
    1,158 weapon-mount samples, 46 facing samples, and zero facing
    mismatches; the copied log is
    `artifacts/remote-role-probe-20260804/CombatRoleFresh.stdout.log`.
    That probe eventually reached `target_eliminated=yes`, so it validates
    role, weapon, and movement presentation but not survival. A separate
    28-second remote null-renderer passive run stayed alive with five enemies,
    44 movement samples, 563 weapon-mount samples, and 26 role-fire events;
    its summary is `artifacts/remote-passive-survival-20260804/`.
    Neither null-renderer result replaces the player-operated capture.

    The critical-health recovery pass is now implemented and verified in the
    exact staged package. The player gets one `1.25s` recovery episode per
    health cycle, with a `10`-health floor, `42`-health cap, and `28` health/s
    recovery rate; hostile damage during the pulse remains authoritative and
    the normal death path is unchanged. The Mac Pro null-renderer bridge run
    reached five hostiles, depleted `30` rounds, restored `30/180` on reload,
    crossed health `28.1`, logged `player critical recovery started`, and
    recovered into the mid-30s while fire continued. Evidence:
    `artifacts/remote-critical-recovery-20260805/`. Keep the player-operated
    OS-keyboard/visual survival gate open.

19. **The level-2 jump launch was unbounded in the older package.** This is
    fixed in the current Mac package at
    `artifacts/package-jump-bounded-20260803/`: the Jump action clears the
    inherited held-jump state on release, rejects airborne/flying and
    runtime-floor-wait states, restores normal gravity, and applies one
    bounded `420 cm/s` launch. A fresh Floodline smoke
    (`-BM_CampaignLevel=1`) recorded `Z=85.150` grounded before the press,
    `Z=139.257` at the peak (`+54.107 cm`, about 1.8 ft), and
    `Z=85.150` grounded after recovery. UE5.1/Win64 and player-operated
    release capture remain open.

20. **Remote Mac Pro full-screen capture remains permission-limited.** On
    2026-08-04 the four remote archive parts were assembled and matched SHA-256
    `d06413918e10192535823b2df83653de11ccc98f5e4d2169f4e6895722120c11`.
    The macOS 13.0.1 / Apple M1 compatibility package passed code-signature
    verification, initialized Metal, created a 1280x748 game window, and its
    rendered run logged five enemy weapon attachments, 124 movement samples,
    44 role-fire events, and Assault/Breacher/Rifleman/Marksman coverage in
    `artifacts/remote-macos13-visual-20260804/`. Later PID-scoped/in-game
    captures are genuine gameplay frames, including environment, HUD, weapon,
    enemies, movement, reload, and muzzle fire, in
    `artifacts/remote-authored-reload-20260804/` and
    `artifacts/remote-rendered-authored-20260804/`. A separate OS full-screen
    capture can still return the desktop or fail when Screen Recording/window
    sharing is unavailable. A fresh Tailscale run passed reload input at
    `0.46s` partial and `0.65s` empty, combat feel, and boundary recovery; its
    game window was on-screen but reported `CGWindowSharingState=0`, so the OS
    capture showed the desktop. Grant that permission before counting a fresh
    full-screen or trailer capture. The existing gameplay-frame captures remain
    valid visual evidence for the weapon, HUD, enemies, movement, reload, and
    muzzle-fire presentation.

    A fresh exact arm64 package run on 2026-08-04 used the working
    `gharchive-pro` Tailscale route. The executable hash is
    `feff016cb9a66f46f1b41e80fdebf88c25f28a9782d16b657820eace103e343c`; all
    six executable/cooked-container hashes are recorded in
    `artifacts/remote-exact-package-20260804/RemotePackage.sha256`.
    Packaged map smoke, combat-role, boundary, and reload checks completed
    against that exact package. The rendered showcase logged
    the authored shell, five-enemy wave, HUD, and controlled shots; OS
    window/full-screen capture remains permission-limited, so this is runtime
    proof rather than a new player-visible screenshot.

    The fresh AI-facing arm64 package at
    `/Users/muhammadluay/Desktop/NightglassSignal-remote-20260804-ai-facing/extracted/NightglassSignalDemo.app`
    is separately hash-verified in
    `artifacts/remote-ai-facing-20260804/RemoteAiFacingSummary.txt`. Its
    extended role run records five enemies, all four roles, five movement
    activations, `51` role-fire events, `124.5`--`434.4 cm` displacement for
    every pawn, and zero facing mismatches. The remote passive process remains
    alive after `28` seconds. These results confirm the movement/weapon/fire
    compatibility contract but do not close player-operated survivability.

21. **Remote playtest input is a development bridge, not player-operated
    capture.** The current package can be launched with the opt-in
    `-BM_RemoteInputPort=1986` flag and accepts newline-delimited `KEY`, `LOOK`,
    and `STATE` requests through an SSH/Tailscale connection. The socket is
    loopback-only on the Mac Pro and the bridge is compiled out of shipping
    builds. On 2026-08-04 the verified remote executable
    (`feff016cb9a66f46f1b41e80fdebf88c25f28a9782d16b657820eace103e343c`)
    loaded Glasswake in the exact fresh package. A held-open bridge moved the
    pawn from `x=0.00 y=-950.00` to `x=226.51 y=-537.48`; PING, STATE, LOOK,
    W, LMB, R, and CLOSE were accepted through the ordinary `InputKey` path.
    The rendered showcase log records five enemy role spawns, hand/muzzle
    weapon mounts, and AI movement. The manual bridge run then reached the
    death screen before a player-shot event could be confirmed, so the
    player-operated opening-wave survivability gate remains open. The fresh
    opening-pacing-fast package also accepted the bridge's `PING`, `STATE`,
    `LOOK`, and `W` requests; the rendered log observed the pawn move from
    `x=0.00 y=-950.00` to approximately `x=130.81 y=-750.52` before that
    short run closed by request. This remains partial input-path evidence,
    not a complete player capture. Evidence is in
    `artifacts/remote-exact-package-20260804/`,
    `artifacts/remote-exact-playtest-20260804/`, and the fresh role/passive
    summaries; the bridge remains a development-only input path and does not
    replace OS-keyboard, screen capture, player-operated 10--15 minute, or
    UE5.1/Win64 release evidence.

    The new rendered run against the AI-facing package also returned `PONG`,
    `STATE`, `LOOK`, `W DOWN/UP`, and `LMB DOWN` through the existing
    loopback SSH forward. A held `W` moved the pawn from `x=0.00 y=-950.00`
    to `x=800.56 y=-280.65`; the rendered log records five enemy spawns,
    continued AI movement, `59` HUD frames, and first-person dry-fire
    presentation. The final `LMB UP`/`STATE` response was lost when the
    bridge client became stale, so this remains partial bridge evidence rather
    than a complete player-operated capture. Evidence:
    `artifacts/remote-ai-facing-20260804/`.

    The stale-client behavior exposed a separate development hazard: a held
    simulated movement key could outlive the SSH/Tailscale client. The rebuilt
    `ABMPlayerController` now releases tracked simulated keys on EOF/error,
    keeps the bridge polling while the world is paused, and clears the single
    client slot. A local UE5.8.1 Mac regression held W, disconnected without a
    key-up, then reconnected and observed the pawn stationary at `speed=0.00`;
    evidence is `artifacts/local-remote-input-regression-20260804/`. The
    isolated rebuilt-package Mac Pro run on ports 1992/1993 repeated the
    disconnect cleanup and accepted replacement-client, LOOK, fire, and reload
    commands; evidence is `artifacts/remote-input-fix-20260804/`. Escape was
    accepted but did not transition this auto-start session to `paused=yes`, so
    paused-screen recovery remains open. None of this is player-operated or
    release evidence.

22. **The remote Mac Pro does not meet the current 60 FPS performance budget.**
    Its fresh 30-second rendered package run produced 846 frames, `28.19` FPS
    average, `37.72 ms` p95 frame time, `26.51` FPS p95, and a `400 ms` maximum
    frame. The result is valid hardware evidence for the 8 GB Apple M1 Pro
    machine, but optimization, scalability profiling, and Windows target
    hardware validation remain open.

23. **Player-operated opening-wave survivability is still not closed on the latest exact Mac Pro package.** On 2026-08-05 the package with executable SHA-256 0927f618f167ff32910bbd40e4c3868bfbf5ce669de1b29c40699f700e53f232 launched through the loopback bridge on port 1987. A held W moved the player to phase 3 with five hostiles. Holding fire changed the live state from magazine 30 to 0, and R restored 30/180 after reload. The restarted checkpoint run then fell from health 100 to 19 and to 0 while two hostiles remained, entering the death screen. The same run directly records five hand_r weapon attachments, changing enemy weapon-mount positions, enemy Fire/ReloadStarted/Hit events, and return fire. This closes the remote weapon/movement/ammo/reload compatibility observation but leaves player-operated survivability open. Evidence: artifacts/remote-opening-survival-state-20260805/.

    That `0927f...` result remains a valid historical failure for the older
    package. The subsequent critical-recovery rebuild is separately verified
    in `artifacts/remote-critical-recovery-20260805/`; it proves the bounded
    recovery contract in the packaged runtime, but it does not replace the
    pending OS-keyboard/player-operated visual capture.

24. **The cap-6 damage-window recheck improves survivability but does not close the player-operated gate.** `BMCombatTuning::HostileDamageWindowCap` is now `6.0f`. The exact rebuilt package at `artifacts/package-player-survival-20260805-tuned/` (executable SHA-256 `e2c0d29be6cd3cbf23eae85ab088538d3254a389887cb637dc173954adcd180e`) was copied to `/Users/muhammadluay/NightglassSignalRemote/package-player-survival-20260805-cap6/NightglassSignalDemo.app`; the Mac Pro passed deep strict codesign verification. The `-nullrhi -NoSound -BM_InteractionProbe` run on port `3013` reached phase 6/OpenCombat with eight hostiles. Stationary bridge observations were health `79.4 -> 42.9 -> 21.5 -> 28.9`, with no elimination across roughly 130 simulated combat seconds; an empty `0/210` state reloaded to `30/180`. The evidence is `artifacts/remote-player-survival-cap6-20260805/`. This is development bridge/null-renderer evidence only; rendered visuals, OS-keyboard/player-operated survival, UE5.1/Win64, and release balance gates remain open.

25. **Death pacing was retuned on 2026-08-13 after the cap-6 recovery behavior still felt too forgiving.** The current source raises the shared hostile damage window from `6` to `18` damage per second and the opening-wave window from `6` to `12`, while shrinking the single critical recovery episode to a `2`-health floor, `8`-health cap, `0.75`-second duration, and `2` health/second. Squad assistance now triggers below `35%` health, restores `2`, and has a `45`-second cooldown. The rebuilt package is `artifacts/package-death-tuning-20260813/`. UE5.8.1 Mac automation passes `35/35`; the packaged combat-role probe records enemy fire and health falling from `100.0` to `23.6`; and the networked death-presentation probe reaches authoritative elimination and replicated knockdown. This is a development/compatibility balance result, not UE5.1/Win64 or player-operated release evidence.

These are deliberately visible limitations. The project does not present
placeholder behavior as completed AAA content.
