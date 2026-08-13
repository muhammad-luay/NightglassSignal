# AI

The source-only AI uses sight/hearing perception plus a bounded state machine.
Behavior Tree, State Tree, EQS, cover-query, and role assets belong here when
the editor pass begins. The generated showcase map has no authored NavMesh, so
enemy controllers currently use `AddMovementInput` as a collision-aware direct
movement fallback after issuing normal `MoveTo` requests. An authored level
should add a NavMeshBoundsVolume and can then replace that fallback with
navigation/cover queries without changing the combat API.

The current combat roster has four readable roles: Rifleman (ARX-4, balanced
mid-range pressure), Assault (Kestrel-9, fast close pressure), Marksman
(Rook DMR, long-range overwatch), and Breacher (Breach-12, fast shotgun
pressure that pushes the player out of cover). Role colour, marker, weapon,
engagement range, movement speed, and grenade cadence are assigned from the
same replicated role enum so the silhouette and behaviour agree on every
client.
