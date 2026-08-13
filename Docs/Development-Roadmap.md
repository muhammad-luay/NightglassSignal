# Development roadmap

## Gate 0 — source foundation (current)

Deliver the isolated UE5.1 project, modular C++ APIs, runtime greybox, source
validation, design/architecture docs, and explicit verification boundary.

## Gate 1 — FPM-01 editor proof

On a provisioned UE5.1 host, fix UHT/compiler issues, open Entry, play the
first-person loop, and record the fire/movement/damage/HUD acceptance.

## Gate 2 — authored combat pass

Create authored weapon data assets, first-person arms, animation state
machines, montages, surface physical materials, muzzle/impact VFX, legal audio,
and AI cover/EQS assets. The source already provides projectile, interaction,
accuracy/sway, and friendly-engagement seams; retain those APIs and replace
only the presentation/data seams.

## Gate 3 — Glasswake mission pass

Author the map, playable cinematics, set piece, checkpoint presentation,
optional intel branch, squad VO/subtitles, extraction, and full 10–15 minute
pacing. Runtime checkpoint/profile save and restoration now have a two-process
UE5.8.1 Mac development proof; UE5.1/Win64 release proof and authored
presentation remain required.

## Gate 4 — Signal Control proof

Add lobby UI, reconnect/disconnect handling, two-client authority tests,
packet-loss/latency test, and a packaged listen/dedicated server run. Source
countdown, bot fill, kill-feed state, latency seam, and visibility/recent-combat
spawn scoring are already in place.

## Gate 5 — release proof

Profile at 1080p on named mid-range hardware, tune scalable presets, complete
accessibility/controller/UMG/audio passes, soak multiplayer, package Win64,
capture the trailer, and archive the exact source/build tuple.
