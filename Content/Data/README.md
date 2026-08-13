# Data assets

The editor automation suite creates the six serialized weapon profiles under
`Content/Data/Weapons/`:

- `DA_BM_ARX4_Rifle`
- `DA_BM_Kestrel9_SMG`
- `DA_BM_Rook_DMR`
- `DA_BM_Breach12_Shotgun`
- `DA_BM_Solace_Pistol`
- `DA_BM_Arc7_Launcher`

Each profile owns the weapon identity, balance values, attachment slots,
third-/first-person mesh references, and the `Grip`/`Muzzle` socket contract.
The runtime fallback profiles in `BMWeaponDataAsset.cpp` are a development
bridge only: gameplay loads these serialized assets first and uses the bridge
when content is missing or not yet cooked.

The throwable profiles live under `Content/Data/Throwables/`:
`DA_BM_Frag_Grenade` and `DA_BM_Veil_Grenade`. They own fuse timing, damage or
suppression radius, throw movement, and optional audio/VFX references. The
runtime actor applies the selected profile before its authoritative fuse is
started.

Create `UBMCharacterAnimationDataAsset`, `UBMAudioDataAsset`,
`UBMVFXDataAsset`, and `UBMSurfaceRuleDataAsset` instances here as their
authored presentation assets become available. `DA_BM_CombatVFX` is now
serialized and wired to all authored weapon and throwable profiles; its
generated Niagara systems are a replaceable functional baseline until bespoke
effects are authored. Keep balance, presentation references, and surface
rules in assets; gameplay actors only consume the profiles.
