# Environment art kit

The environment package is generated and saved by the editor automation test
`BlackMeridian.World.EnvironmentAssets`. It contains the project-owned
industrial block, pipe, orb, deck, and material instance used by both saved
maps and the runtime compatibility director.

The meshes intentionally preserve the dimensions of the old BasicShapes so
collision, cover lanes, and mission interaction coordinates remain stable
while the presentation gains chamfered edges, service ribs, collars, a
grated deck language, modular overhead trusses, and raised service grates.

Runtime props also compose these assets into signal towers. Towers are
landmarks rather than collision-critical cover: a deck base, structural block,
raised grate, truss head, mast, orb beacon, and warning lamp establish scale
and make the cyan/amber route language readable from the combat lanes.

The current kit is:

- `SM_BM_Env_Block`: chamfered structural block with raised service ribs.
- `SM_BM_Env_Pipe`: capped industrial pipe with side brackets.
- `SM_BM_Env_Orb`: signal lamp/marker silhouette.
- `SM_BM_Env_Deck`: heavy modular deck tile.
- `SM_BM_Env_Truss`: repeatable overhead service gantry for horizon scale.
- `SM_BM_Env_Grate`: raised slatted transition with real collision.
- `SM_BM_Env_Panel`: shallow ribbed maintenance wall module for facades and
  relay cabinets.
- `SM_BM_Env_Cover`: chamfered low-cover module with impact lips and feet for
  readable barricade silhouettes.
- `SM_BM_Env_Door`: framed service door with recessed leaf, handle rail, and
  threshold for readable human-scale entrances.
- `SM_BM_Env_Window`: observation-window frame with mullions and recessed pane
  for facade depth and interior sightlines.
- `SM_BM_Env_Sign`: elevated facility sign with supports and beacon cap for
  route navigation and landmark identity.
- `SM_BM_Env_Machinery`: single low-poly relay machine silhouette combining a
  tank, service console, cable collars, and signal head.
- `SM_BM_Env_Wreckage`: one optimized collapsed-hull silhouette with a low
  armored keel, broken cab, exposed ribs, and collision suitable for flank
  cover. It is used as structural battlefield geometry in Sable Quay, Ashen
  Docks, Wreckline, and Last Light rather than as decorative dressing.

## Campaign environment themes

Every one of the twenty campaign entries has a unique profile ID, terrain ID,
structural layout ID, prop set, sky treatment, and ambience route. Campaign
play suppresses the generic Glasswake shell and builds the selected layout as
real collision-bearing runtime geometry. The director still keeps an
invisible lower floor and perimeter safety colliders so the campaign cannot
end because of an accidental fall:

- Sable Quay: open dry ridge and clear blue horizon.
- Floodline and Storm Vault: flooded drainage, high catwalks, heavy rain,
  storm fog, and lightning-grid dressing.
- Relay House, Cinder Works, Glass Market, Iron Cathedral, and Ember Station:
  rooftop, foundry, arcade, turbine, and fuel-station interiors.
- Nightglass Core: underground signal tunnels with violet low light and mist.
- Salt Foundry, Quarry Nine, and Vanta Spire: salt flats, desert canyon, and
  a wind-exposed service ridge with dust treatment.
- Lantern Basin and Meridian Gate: damp forest and cold gate approaches.
- Pale Harbor: snow and ice berths with a whiteout sky; Last Light adds a
  separate snow-grade ridge treatment.

The runtime director applies the profile's floor/room/canyon/catwalk layout,
ground palette and material response, sun angle, sky light, fog density,
post-process grade, practical lights, collision-bearing structures,
non-collision prop signature, ambient bed, and weather field. The
instanced weather field is deterministic per level and the profile explicitly
routes its authored Niagara system (`NS_BM_Rain`, `NS_BM_Snow`, `NS_BM_Dust`, or
`NS_BM_Smoke`). Coastal storm profiles also enable a short, non-gameplay
lightning flash driven by the local weather clock. All visual dressing is
tagged as non-collision and leaves the validated gameplay shell authoritative.

The four wreckage placements are part of the collision-bearing layout pass.
They are intentionally offset from the objective route so they create a
readable flank/cover choice without closing the required traversal lane. The
layout log reports `wreckage_pieces` so packaged probes can verify the asset was
actually instantiated.

The family treatment is intentionally broad while the structural layout is
per-level:
open dry, heavy storm, snow/ice, desert, forest, underground, and interior
levels each carry their own terrain, layout, sky, fog, prop-set, ambience, sun
grade, and weather route. Selecting a different campaign entry therefore
changes the actual traversal space as well as its presentation; the hidden
fail-safe floor and perimeter are the only shared safety contract.
