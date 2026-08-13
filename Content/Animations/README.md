# Animation contract

`UBMCharacterAnimationDataAsset` and `IBMCharacterAnimationInterface` provide
the replaceable authored layer. A character can supply an AnimBP class,
locomotion blend space, aim-offset blend space, and named montages for idle,
walk, sprint, tactical sprint, crouch, prone, slide, jump/land, fire, ADS fire,
reload, weapon switch, grenade, melee, hit reaction, and death.

`ABMCombatCharacter` publishes movement state, ground speed, aim offset, and
named events. `ABMPlayerCharacter` exposes a separate owner-only first-person
arms component while the inherited character mesh remains the third-person
slot. The current staged compatibility build supplies a project-local Manny
mesh and rifle locomotion/event clips; the runtime placeholder path remains a
safe fallback when those cooked assets are unavailable.
