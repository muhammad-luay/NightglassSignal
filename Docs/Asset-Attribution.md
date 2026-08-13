# Asset attribution

| Asset/source | Use | License / status |
| --- | --- | --- |
| Unreal Engine 5.1 BasicShapes (`/Engine/BasicShapes/Cube`) | runtime greybox geometry and placeholder weapon/body meshes | Requires a licensed Unreal Engine installation; no engine files are redistributed |
| Black Meridian low-poly weapon meshes (`Content/Weapons/Meshes/SM_BM_ARX4_Rifle`, `SM_BM_Kestrel9_SMG`, `SM_BM_Rook_DMR`, `SM_BM_Breach12_Shotgun`, `SM_BM_Solace_Pistol`, `SM_BM_Arc7_Launcher`) | project-owned third-person firearm presentation and replaceable first-person/AI weapon assets | Original generated project geometry; development placeholder material uses the licensed Unreal BasicShapes material |
| Unreal Engine Mover Examples plugin | Not enabled or force-cooked by the current project/package; retained only as an installed-engine provenance note for older editor captures | Uses the locally installed licensed Unreal Engine plugin; no plugin content is redistributed by the current package; older captures that relied on it are not release evidence |
| Unreal Engine mannequin body/materials/rigs (`Content/Characters/Mannequins/Meshes/SKM_Manny_Simple`, `Materials`, `Textures`, `Rigs`) | Project-cooked development enemy body and physics presentation | Copied from the locally installed UE5.8 TemplateResources mannequin content for this licensed UE project; keep development-only and replace or re-clear for portable/release distribution |
| Unreal Engine mannequin rifle clips (`Content/Characters/Mannequins/Anims/Rifle`, `SK_Mannequin`) | Development-only two-handed rifle-ready, locomotion, fall, fire, reload, equip, and dry-fire presentation | Copied from the locally installed UE5.8 TemplateResources mannequin content for this licensed UE project; keep development-only and replace or re-clear for portable/release distribution |
| Unreal Engine default fonts | runtime-authored UMG and legacy canvas HUD text | Requires the licensed engine; not copied into this repository |
| C++ generated geometry/material parameter values | cover, walls, lights, placeholder colors | Original project code, MIT repository license |
| C++ debug line/point drawing | tracer and hit feedback during prototype | Original project code; development placeholder |
| External art/audio/voice | none currently included | No third-party asset is claimed or redistributed |
| Optional audio routing assets | `UBMGameUserSettings.AudioMix` and Sound Class references | Empty by default; authored project assets must be attributed when added |

When an external placeholder is added, record creator, source URL, asset ID,
license, modification, and redistribution permission here before committing it.
Final art, sound, animation, and music must be replaced only with owned,
generated, or explicitly licensed material.
