# Levels

The project now contains two saved, authored greybox shells:

- `Glasswake.umap` — the single-player Operation Glasswake insertion,
  checkpoint, breach, arena, and extraction route.
- `SignalControl.umap` — the compact multiplayer arena with perimeter starts,
  three node landmarks, interior cover, and vertical route cues.

The maps own the multiplayer shell, base lighting, player starts, and
navigation bounds. For single-player campaign operations,
`ABMGreyboxDirector` suppresses the generic Glasswake mesh shell and replaces
it at runtime with the selected level's collision-bearing structural layout.
`ABMGreyboxDirector` and
`ABMSignalControlGameMode` still own replicated mission/match state and spawn
runtime interactables, enemies, friendlies, bots, and control nodes on top of
the shells. This keeps gameplay logic independent of final art while ensuring
the saved maps are what players and cooked builds load.
