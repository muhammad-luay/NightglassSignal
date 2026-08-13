# VFX architecture

`UBMVFXDataAsset` holds replaceable Niagara slots for muzzle flashes, impacts,
optional blood, explosions, dust, smoke, sparks, shell ejection, and
suppression, plus an optional impact decal material. The current runtime path
spawns configured weapon, projectile, and throwable systems on non-dedicated
clients, while debug lines/points/spheres keep the greybox readable when no
assets are configured. Gore reduction suppresses the optional blood slot.

The editor presentation test now creates the project-owned
`/Game/Data/VFX/DA_BM_CombatVFX` profile and nine serialized systems under
`/Game/VFX/Generated/`. They are duplicated from UE Niagara's deterministic
burst templates so the packaged combat path has real cooked Niagara systems
for muzzle, impact, explosion, smoke, dust, sparks, shell, and suppression
events. They are a functional presentation baseline, not final bespoke VFX;
replace the generated systems with authored effects without changing the
runtime slots.

Weather has dedicated project-owned accent systems in addition to the
deterministic runtime field:

- `NS_BM_Rain`: rain/storm accent for flooded yards, seawalls, and catwalks.
- `NS_BM_Snow`: snow/ice accent for Pale Harbor and the late snow-grade route.

The director selects these by `EBMWeatherType`; rain and snow continue to use
the instanced field as the continuous fallback so a missing optional Niagara
package never removes the weather presentation entirely.
