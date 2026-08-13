# NightglassSignal campaign level review

Date: 2026-08-11

## Evidence and scope

This review covers the 20 PNGs in
`artifacts/campaign-level-screenshots-20260811/`. Each image was captured from
the normal packaged campaign entry path by
`Scripts/capture_campaign_level_screenshots.sh` using `Glasswake`,
`BM_AutoStartMission`, and one `BM_CampaignLevel` value per level. The capture
did not use the combat showcase, a teleport, a synthetic wave, or the forced
fallback-enemy presentation.

The capture set proves that all 20 campaign selections load, replace the
shared shell with a level-specific runtime layout, initialize collision and
navigation, and apply a level-specific environment profile. It does not by
itself prove a full player-visible traversal, combat encounter, objective
completion, extraction, enemy movement, or death presentation: every PNG is an
insertion frame with `00 HOSTILES` and the first `MOVE TO THE TEAL BEACON`
objective.

There is separate functional evidence in
`artifacts/playtest-full-campaign-todos-fixed-20260811/`: all 20 logs contain
`traversal=complete combat=complete objectives=relay+console extraction=boat
final_defense=complete phase=Complete`. That is a null-renderer automation
probe, so it proves the gameplay contract rather than the visible quality of
the encounter. The aggregate probe records 848 project-local Manny mesh loads
and 848 `hand_r` weapon attachments, 7,212 navigation-path events, and zero
direct-fallback movement events. If the live game still shows boxes or no
Manny, that is a player-visible package/presentation regression or a stale
launch path; it is not resolved by the insertion screenshots. The intentional
box diagnostic used `BM_ForceFallbackEnemyPresentation` and must not be used as
normal-campaign evidence. The current Manny is still a greybox/placeholder
character and should be replaced by authored enemy art before release.

## Verified combat presentation fix — 2026-08-11

The packaged normal combat capture now passes with fallback disabled. The early
frame visibly shows the project Manny mesh with a readable black ARX-4 rifle
held at the right hand; the mid and late frames show the same Manny entering a
real death pose rather than becoming a moving box. Runtime evidence confirms
the project Manny mesh, rifle AnimBP, `hand_r` attachment, project weapon asset,
weapon mount/muzzle coordinates, and five accepted player shots. The combat-role
probe also reports five enemy roles, 49 role-fire events, 12 movement events,
and zero movement/facing mismatches. Evidence:

- [early combat capture](artifacts/packaged-combat-showcase-weapon-visibility-offset-20260811/20260811-142933/combat-showcase-early.png)
- [mid combat capture](artifacts/packaged-combat-showcase-weapon-visibility-offset-20260811/20260811-142933/combat-showcase-mid.png)
- [late combat capture](artifacts/packaged-combat-showcase-weapon-visibility-offset-20260811/20260811-142933/combat-showcase-late.png)
- [capture log](artifacts/packaged-combat-showcase-weapon-visibility-offset-20260811/20260811-142933/CombatShowcase.stdout.log)
- [combat-role probe](artifacts/combat-role-probe-weapon-visibility-offset-20260811/CombatRoleProbe.result.txt)

The source change keeps the weapon muzzle and trace aimed at the target while
moving the third-person rifle slightly to the shoulder side, forward, and down
so the firearm remains visible from the player camera. `python3
Scripts/validate_nightglass.py` passes after the change.

## Level-by-level review

The structural and prop values below are taken from the matching stdout log;
the visual notes are from the matching full-resolution PNG.

| Level | Screenshot | Physical battlefield and environment evidence | Visual/gameplay improvement |
| --- | --- | --- | --- |
| 01 — Sable Quay | [view](artifacts/campaign-level-screenshots-20260811/01-SableQuay.png) | `LAYOUT_OPEN_DRY_RIDGE`; 56 structural pieces, 5 wreck pieces, 18 props, 1 vehicle, 2 hard-cover pieces; `DRY_RELAY_YARD`, `SKY_CLEAR_BLUE`, no weather. | The open relay yard has a broad exposed perimeter and repeated low cover. Add a recognizable relay building or ship hull, staggered medium cover, and two readable approach lanes around the beacon. |
| 02 — Floodline | [view](artifacts/campaign-level-screenshots-20260811/02-Floodline.png) | `LAYOUT_FLOODED_DRAINAGE_YARD`; 11 structural pieces, 18 props, 2 vehicles, no hard-cover pieces; `FLOODGATE_STORMWORKS`, storm-front sky, rain with lightning. | Pipes and a large overhead slab dominate a dark, narrow view, while the flooded-drainage identity is not visually clear. Add channels, water edges, bridges, low concrete cover, and a lit floodgate landmark; preserve a clearly safe raised route. |
| 03 — Relay House | [view](artifacts/campaign-level-screenshots-20260811/03-RelayHouse.png) | `LAYOUT_ROOFTOP_RELAY`; 39 structural pieces, 18 props, 1 pillar, 2 furniture pieces, 1 hard-cover piece; `ROOFTOP_SERVICE`, overcast-city sky, smoke. | A giant pale wall fills the approach and the left side falls into darkness, making the rooftop route and entrance hard to read. Add rooftop rooms, doors, guardrails, stairs, and a visible alternate service path; reduce the wall obstruction and lift interior light. |
| 04 — Nightglass Core | [view](artifacts/campaign-level-screenshots-20260811/04-NightglassCore.png) | `LAYOUT_UNDERGROUND_CORE`; 60 structural pieces, 20 props, 4 pillars, 1 furniture piece, 1 hard-cover piece; core-glow sky, smoke. | The right half is nearly black and the player faces an oversized wall and pipe cluster with little floor guidance. Add emissive core machinery, tunnel frames, floor lights, readable junctions, and low cover so the underground route is legible without removing the darkness. |
| 05 — Ashen Docks | [view](artifacts/campaign-level-screenshots-20260811/05-AshenDocks.png) | `LAYOUT_CINDERED_DOCKS`; 9 structural pieces, 1 wreck piece, 18 props, 2 vehicles, 2 hard-cover pieces; burning gantries, red smoke sky, dust. | The screenshot reads as an open greybox deck with a large empty left horizon and scattered fire/debris effects. Add dock cranes, cargo stacks, a ship hull or collapsed pier, and solid cover that creates a cindered lane instead of one exposed crossing. |
| 06 — Cinder Works | [view](artifacts/campaign-level-screenshots-20260811/06-CinderWorks.png) | `LAYOUT_FOUNDRY_KILNS`; 46 structural pieces, 19 props, 1 vehicle, 3 pillars, no hard-cover pieces; hot-casting line, furnace-glow sky, smoke. | The camera is pressed against dark kiln walls with only a thin slit ahead; the foundry machinery is not readable and the route looks potentially blocked. Add kiln chambers, catwalks, ramps, heat pools, and actual cover; test the opening for both player and enemy clearance. |
| 07 — Salt Foundry | [view](artifacts/campaign-level-screenshots-20260811/07-SaltFoundry.png) | `LAYOUT_SALT_EVAPORATION_BASIN`; 15 structural pieces, 21 props, 1 pillar, 6 hard-cover pieces; salt depot, white-haze sky, dust. | The left half is an empty flat expanse and the meaningful cover is concentrated on the right, creating a long exposed sightline. Add evaporation pans, berms, service tracks, and a second cover lane; keep the white dust trail visible as route guidance. |
| 08 — Blackwater Yard | [view](artifacts/campaign-level-screenshots-20260811/08-BlackwaterYard.png) | `LAYOUT_OIL_TANK_FARM`; 14 structural pieces, 18 props, 2 vehicles, 1 hard-cover piece; pump station, oil-mist sky, rain with lightning. | Tanks and pipes establish the theme, but the yard is very dark and the open right lane has little cover or landmarking. Add tank ladders, pipe runs, pump housings, lit control equipment, and cover around the route without making every sightline identical. |
| 09 — Glass Market | [view](artifacts/campaign-level-screenshots-20260811/09-GlassMarket.png) | `LAYOUT_SHATTERED_MARKET`; 23 structural pieces, 22 props, 6 furniture pieces, 2 hard-cover pieces; signal mirrors, magenta-dusk sky, no weather. | A huge magenta wall and overhead beams obscure the market and leave only a narrow opening. Add stalls, facade fragments, signs, and alternate arcade alleys; resize or move the blocking wall so the objective direction remains visible. |
| 10 — Storm Vault | [view](artifacts/campaign-level-screenshots-20260811/10-StormVault.png) | `LAYOUT_HIGH_STORM_CATWALKS`; 60 structural pieces, 18 props, 2 pillars, 1 hard-cover piece; lightning grid, electric-storm sky, rain with lightning. | The catwalk network has large voids and heavy overhead rails that cut through the view and make vertical traversal/fall safety unclear. Add stairs, ramps, intermediate landings, guardrails, and a landmarked safe route; keep the objective banner clear of the overhead structure. |
| 11 — Red Tide | [view](artifacts/campaign-level-screenshots-20260811/11-RedTide.png) | `LAYOUT_CHEMICAL_SEAWALL`; 14 structural pieces, 21 props, 2 pillars, 4 hard-cover pieces; filter stations, red-tide sky, rain with lightning. | The seawall is represented by tall panels and pipes, but the right side reads as an empty void and a central wall hides the route. Add filter tanks, valves, breakwater sections, edge rails, and a visible bridge/door sequence with cover before the hold area. |
| 12 — Iron Cathedral | [view](artifacts/campaign-level-screenshots-20260811/12-IronCathedral.png) | `LAYOUT_TURBINE_AISLES`; 26 structural pieces, 20 props, 5 pillars, 1 furniture piece, no hard-cover pieces; iron relay shrine, turbine-vault sky, smoke. | The repeated bright pillars make a dense sightline maze against a nearly black background, and there is no obvious cover despite the power-station theme. Add turbine bodies, control rooms, aisle landmarks, and low cover; tune the contrast so enemies cannot disappear into the vault. |
| 13 — Lantern Basin | [view](artifacts/campaign-level-screenshots-20260811/13-LanternBasin.png) | `LAYOUT_DAMP_FOREST_BASIN`; 27 structural pieces, 23 props, 8 vegetation pieces, 1 hard-cover piece; lantern chain, forest-mist sky, smoke. | This is one of the clearest theme changes, but many lantern/vegetation forms look oversized or suspended and clutter the route. Ground the trees and lanterns, add terrain height changes and foliage cover, and reserve a clean lantern-marked path through the basin. |
| 14 — Quarry Nine | [view](artifacts/campaign-level-screenshots-20260811/14-QuarryNine.png) | `LAYOUT_DESERT_SIGNAL_CANYON`; 10 structural pieces, 18 props, 2 pillars, 2 hard-cover pieces; excavator rigs, dust-canyon sky, dust. | A large overhead slab creates a tunnel-like approach and dominates the sightline; the canyon walls and excavator identity are not yet strong. Add actual cut rock faces, ramps, excavation machinery, stacked cover, and a clearly marked underpass/alternate route with safe headroom. |
| 15 — Vanta Spire | [view](artifacts/campaign-level-screenshots-20260811/15-VantaSpire.png) | `LAYOUT_SPIRE_SERVICE_RIDGE`; 55 structural pieces, 18 props, 2 pillars, 1 hard-cover piece; spire radar towers, violet-dawn sky, no weather. | The elevated service ridge has a large overhead mass, a narrow central walkway, and an empty drop-side perimeter. Add tower stairs, landings, railings, service bridges, and a readable ascent loop; make the non-failing boundary visually obvious. |
| 16 — Wreckline | [view](artifacts/campaign-level-screenshots-20260811/16-Wreckline.png) | `LAYOUT_CONTAINER_WRECKLINE`; 24 structural pieces, 1 wreck piece, 19 props, 2 vehicles, 3 hard-cover pieces; dead radar hulks, salt-squall sky, rain with lightning. | The container-hull theme is present, but a very dark container fills the center and leaves a single narrow opening toward the beacon. Add varied container stacks, open doors, cargo cover, and a second gap; light the beacon-side route without removing the maze character. |
| 17 — Ember Station | [view](artifacts/campaign-level-screenshots-20260811/17-EmberStation.png) | `LAYOUT_FUEL_SERVICE_RACKS`; 9 structural pieces, 19 props, 2 vehicles, 2 hard-cover pieces; ignition tanks, heat-shimmer sky, dust. | Most physical dressing is on the right while the left is a large empty perimeter, so the fuel-stop identity and western-tank objective are weak. Add tanks, hoses, pipe racks, service platforms, flame/isolation barriers, and a clearly lit route through the station. |
| 18 — Pale Harbor | [view](artifacts/campaign-level-screenshots-20260811/18-PaleHarbor.png) | `LAYOUT_ICE_BERTHS`; 47 structural pieces, 23 props, 1 vehicle, 2 pillars, 6 hard-cover pieces; frozen cranes, polar-whiteout sky, snow. | A huge bright overhead structure dominates the frame and the white-on-white presentation loses depth and cover edges. Add ice cracks, berth walls, crane bases, ship hulls, and dark/colored navigation landmarks; tune snow and exposure so the safe route remains readable. |
| 19 — Meridian Gate | [view](artifacts/campaign-level-screenshots-20260811/19-MeridianGate.png) | `LAYOUT_GATE_CHECKPOINT`; 11 structural pieces, 19 props, 1 vehicle, 1 pillar, 1 furniture piece, 2 hard-cover pieces; lattice convoy, cold-city sky, smoke. | The gate canopy and columns are visible, but the understructure is dark, the center is cluttered, and the right side reads as empty boundary space. Build checkpoint lanes, barriers, convoy vehicles, gate doors, and a clear central breach/alternate bypass with better lighting. |
| 20 — Last Light | [view](artifacts/campaign-level-screenshots-20260811/20-LastLight.png) | `LAYOUT_LAST_LIGHT_RIDGE`; 11 structural pieces, 1 wreck piece, 19 props, 1 vehicle, 1 pillar, 2 hard-cover pieces; last-light pier, golden-lastlight sky, snow. | The left has a small cluster of dressing while the right is largely empty and pale; floating props and the final pier/extraction space are not visually distinct. Add a shoreline/pier silhouette, final hold cover, extraction lighting, and a strong beacon that remains readable through snow. |

## Cross-level findings and prioritized improvements

### P0 — resolve before calling the presentation complete

- [resolved 2026-08-11] Capture a normal, player-visible encounter after the
  insertion wave has spawned. The packaged combat capture now shows the
  project Manny mesh, role accessory kit, visibly held ARX-4 rifle, real death
  pose, and accepted combat shots with no
  `BM_ForceFallbackEnemyPresentation`; the combat-role probe confirms enemy
  firing and forward-facing movement. Per-level visual encounter captures are
  still a follow-up item for the 20-level presentation pass.
- Keep the functional boundary guard, but add visible rails, walls, shoreline,
  and edge landmarks where the screenshots show a void. Falling must continue
  to recover the player rather than end the campaign; the visual boundary must
  not be mistaken for a missing floor.
- Add a post-clear state transition that says exactly where to go next and
  points to the next beacon/extraction. The insertion banner is readable, but
  the same visual hierarchy must remain after hostiles are gone.

### P1 — improve combat readability and route quality

- Spread hard cover and short sightline breaks into the levels with zero or
  very low hard-cover counts, especially Floodline, Cinder Works, Iron
  Cathedral, Blackwater Yard, and the three sparse dock/fuel layouts.
- Replace oversized blocking slabs, walls, and pillars with deliberate rooms,
  corridors, openings, stairs, ramps, platforms, and alternate routes. Every
  vertical route needs a landing, a guardrail or recovery surface, and a clear
  navigation landmark.
- Add sightline-aware enemy spawns and encounter staging after the structural
  pass. The functional probe passes, but static insertion views do not show
  whether the first encounter starts behind meaningful cover or in an exposed
  lane.
- Rebalance lighting and fog: several interiors are near-black, while Pale
  Harbor and Last Light lose depth to bright surfaces and snow. Preserve the
  environment identity while raising contrast on floors, cover edges, enemy
  silhouettes, weapon pickups, and beacons.
- Make weather support the setting without hiding gameplay. Rain, dust, smoke,
  and snow should have controlled density near the camera and should not erase
  the objective marker, muzzle flash, tracer, or cover silhouette.

### P2 — replace greybox presentation

- Replace BasicShape-style walls, pipes, spheres, floating panels, and
  placeholder materials with authored terrain, buildings, vehicles, props,
  vegetation, debris, and environment-specific ground materials. The current
  layouts are physically different, but the screenshots still read as one
  runtime greybox kit viewed under different profiles.
- Replace the project-local Manny placeholder with authored enemy models or
  strongly differentiated role kits. Keep the existing `hand_r`, `Grip`, and
  `Muzzle` contracts so weapons remain visibly held and fire from the weapon.
- Reduce the persistent objective panel's visual dominance and prevent it from
  covering route-critical geometry. Use a compact top-center distance, compass
  direction, beacon beam, and local landmark rather than a large banner alone.

## Verification checklist for the next build

- [ ] Run the normal packaged campaign path for all 20 levels with no fallback
      flags.
- [ ] Capture insertion, first encounter, post-clear objective, and extraction
      frames for each level.
- [ ] Confirm the visible enemy is a skeletal character, visibly holds the
      selected weapon, advances in the direction it faces, fires from its
      muzzle, reacts to hits, and uses a non-box death presentation.
- [ ] Confirm weapon/ammo pickups auto-collect on proximity and the HUD shows
      the granted weapon/ammo.
- [ ] Confirm each layout has at least one safe primary route, one alternate
      route where intended, usable cover, valid collision, and a visible
      recovery boundary.
- [ ] Re-run the full functional probe and retain the exact per-level marker:
      `traversal=complete combat=complete objectives=relay+console
      extraction=boat final_defense=complete phase=Complete`.
- [ ] Re-run `python3 Scripts/validate_nightglass.py` and the screenshot/link
      count checks after any layout or presentation change.
