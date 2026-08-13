# Content contract

The vertical slice currently uses runtime-generated cubes, lights, and debug
tracers so it can be opened without checked-in binary assets. The runtime
placeholder boundaries are deliberately stable:

- `Content/Characters`: first-person camera/arms and third-person body slots.
- `Content/Weapons`: `UBMWeaponDataAsset` profiles and attachment slots.
- `Content/AI`: Behavior Tree/State Tree/EQS assets may replace the equivalent
  C++ perception state machine without changing the public character API.
- `Content/Levels`: authored maps can replace the generated `ABMGreyboxDirector`.
- `Content/UI`, `Content/Audio`, and `Content/VFX`: authored assets can replace
  the runtime-authored UMG surface, generated audio cues, debug traces, and
  placeholder geometry.

No external art, audio, or generated `.uasset`/`.umap` files are claimed as
complete until they are authored and reviewed in a provisioned UE5.1 editor.
