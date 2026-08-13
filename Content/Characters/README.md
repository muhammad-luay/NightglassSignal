# Character presentation

`ABMCombatCharacter` owns gameplay health/team state and a replaceable body
slot. `ABMPlayerCharacter` hides the body for the first-person view and exposes
an owner-only `FirstPersonArms` skeletal component. `AuthoredCharacterMesh`
and `AnimationData` are the third-person replacement slots; a successfully
loaded mesh hides the runtime cube automatically.

## Current state

The default enemy uses the project-cooked `SKM_Manny_Simple` skeletal mesh
and the project-owned rifle animation subset in `Mannequins/Anims/Rifle`: a
two-handed ready pose, rifle walk/jog/fall clips, and weapon event clips. The
runtime uses this stable project path first, so packaged builds keep the same
soldier body while the AI visibly holds the weapon as it advances,
investigates, flanks, retreats, and fires. The installed UE5.8 `MoverExamples`
Manny remains a compatibility fallback for editor-only setups. These are
development presentation assets; replace or re-clear them before a
portable/release build. On a machine without either body or the clips, the
runtime deliberately falls back to the colored BasicShapes modular rig.

This is a development presentation, not the final Black Meridian soldier art.
The final build still needs an owned or explicitly licensed military
character package imported into the project.

## Import path

1. Import an owned or explicitly licensed skeletal character into this folder.
2. Create a Blueprint child of `ABMEnemyCharacter`.
3. Assign its `AuthoredCharacterMesh`, `AnimationData`, AnimBP, locomotion
   blend space, and death/hit/fire/reload montages.
4. Set `ABMGameMode > EnemyCharacterClass` to that Blueprint. All mission and
   showcase waves will then spawn the authored enemy while retaining the same
   health, weapon, AI, and replication code.

The runtime cube remains the safe fallback if the mesh or animation asset is
missing or fails to load. `AuthoredAnimBlueprint` is the lightweight C++
development fallback when a full `UBMCharacterAnimationDataAsset` has not yet
been authored.
