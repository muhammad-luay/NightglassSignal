# Audio architecture

`UBMAudioDataAsset` is the routing contract for indoor/outdoor/suppressed
gunshots, impacts, footsteps, reload/equipment, explosions, positional
dialogue, ambience, music, low-health, and suppression cues. `UBMWeaponDataAsset`
can optionally reference an audio profile; fire/reload events call the profile
on non-dedicated clients and fall back to the direct weapon sound references.
Surface-specific impact maps and detonation cues are optional extensions of the
same profile. Every future authored cue must be recorded in
`Docs/Asset-Attribution.md`.

Campaign music is phase-driven: `Mission`, `Suspense`, `Combat`, `Failure`,
and `Complete` are selected by `ABMMissionDirector`. `Suspense` is a distinct
slow low-drone/heartbeat bed used during insertion, approach, breach, intel,
and extraction; it is not the neutral mission loop. If an authored
`MusicStates` entry is unavailable, the local player controller generates a
small loop-safe stereo synth cue at runtime, so the playable build has music
before licensed/audio-team assets are imported. `-NoSound` still disables the
fallback for automation and headless runs. The existing Music Volume and
Master Volume settings are applied to the fallback; an authored sound mix
remains the authority when configured.

Campaign environment beds are routed through
`UBMAudioDataAsset::EnvironmentAmbiences`. There is one generated, loopable
ambience bed per campaign entry (`BM_Ambient_*`) covering dry wind, rain,
machinery, forest air, desert dust, underground hum, and snow/ice wind. Phase
music remains separate from these low-volume environment beds, so changing
levels changes the physical soundscape without replacing the mission/combat
music state machine.
