# Black Meridian — game design document

## Identity

**Black Meridian** is a grounded near-future military thriller set in Kestrel
Reach, a coastal industrial district whose emergency communications network has
been quietly weaponized. The player is callsign **Nightglass**, a Lattice
Response Unit operator. The opposing force is the **Vanta Continuum**, a private
security and logistics network that has turned disaster-response infrastructure
into a coercion system.

Lattice uses muted blue-grey equipment, disciplined two-person movement, and
short bursts of coordinated pressure. Vanta uses rust-red identifiers, cheap
commercial drones, layered checkpoints, suppressive fire, and deliberate
withdrawals. Neither side is cartoonishly heroic: Lattice is trying to prevent
a regional panic, while Vanta believes controlled scarcity is the only way to
keep the coast from collapsing.

## Core loop

1. Read the space and choose a route.
2. Move with weight and control: sprint, crouch, lean, mantle, or slide.
3. Identify threats through sight, sound, and squad callouts.
4. Break contact or win the angle with data-driven weapons and limited utility.
5. Secure the next objective, make the optional intelligence choice, and reach
   the extraction/defense space.
6. In Signal Control, rotate between routes, contest the active node, and trade
   kills for time without exposing the next spawn.

## Single-player mission: Operation Glasswake

Lattice inserts through a flooded service channel after Vanta seizes the Sable
Quay relay house. The team expects a communications theft. The first quiet
approach reveals that the relay is broadcasting a pattern rather than storing
one. At the checkpoint, Vanta deliberately escalates and uses the seawall to
seal the district.

The mission spaces are modular and runtime-greyboxed today:

1. **Insertion channel** — short cinematic subtitle/opening and controlled
   movement introduction.
2. **Quiet yard** — sightline management, cover, and squad formation.
3. **Checkpoint** — the first open combat encounter.
4. **Relay-house breach** — interior cover, interaction prompt, and a door/entry
   replacement seam.
5. **Signal archive** — optional hold-to-interact download that changes the
   ending information.
6. **Seawall arena** — reinforcements, suppression, and a readable set-piece
   escalation.
7. **Extraction pier** — route choice and a timed transition.
8. **Final defense** — hold the pier while the boat approaches; ending subtitle
   establishes the wider conflict.

The mission director preserves control during transitions. Cinematic intent is
currently represented by subtitle timing, objective changes, lighting, and
environmental state; authored camera tracks, voice performances, and final
destruction assets are replacement work, not silently claimed as complete.

## Multiplayer: Signal Control

Signal Control supports 2–12 players in two teams, Lattice and Vanta. Three
communications nodes occupy distinct short, medium, and long sightline routes.
Exactly one node is active at a time. A team earns one point per second while it
controls the active node; the node rotates on a fixed timer and the first team
to the configured limit wins.

The game mode owns state on the server: team assignment, node activity,
overlap-based contesting, score, rotation, and results. The player pawn and
weapon RPCs are replicated actors with server-side ammo, fire-rate, shot-origin,
damage, and score validation. The project leaves a clean seam for dedicated
server sessions, listen-server tests, bots, and a future replication graph.

## First playable milestone

**FPM-01: Greybox fire-and-move slice.** From the UE5.1 Entry map, a local
player presses Enter, enters a generated first-person greybox, moves with WASD,
aims/fires the ARX-4, reloads, sees ammo/health/objective HUD feedback, kills a
perception-driven Vanta rifleman, and reaches the first mission transition.
The milestone is complete only after a provisioned UE5.1 editor build and a
packaged development run pass the movement, fire-rate, reload, damage, and HUD
checks in [QA-Checklist](QA-Checklist.md).
