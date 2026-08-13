# Source layout

The thin game module is `NightglassSignalDemo`. Reusable systems live in the
`BlackMeridianRuntime` plugin so dedicated servers do not depend on editor or
presentation code. The plugin's public types are grouped by prefix and
responsibility: `BMWeapon*`, `BMCombat*`, `BMEnemy*`, `BMGame*`, `BMMission*`,
and `BMControl*`. This is the UE-friendly equivalent of separate Core,
Characters, Weapons, AI, Interaction, Missions, Multiplayer, UI, Audio, and
SaveSystem folders without duplicating module boundaries.
