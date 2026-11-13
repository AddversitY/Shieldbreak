# Paragon: Wraith (Fab, Epic Games): inspection

Vincent added the asset to his Fab library. It was added to a throwaway project, `C:\Users\addve\Documents\Unreal Projects\WraithStaging`
(UE 5.8, outside the Shieldbreak repo), under `Content/ParagonWraith`: 1,597 files, about 1.28 GB.

- **Licence:** the listing says it's free to use in Unreal Engine projects. You may not use the PARAGON trademark to name or advertise the game.
- **Engine versions:** UE 4.19 to 5.8, according to the listing's asset-format data.

## Character

| Skin | Skeletal mesh | LODs | LOD0 tris | LOD0 verts | Material slots |
|---|---|---|---|---|---|
| Default (white and grey, cyan visor) | `/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith` | 5 | 83,330 | 42,330 | 4: lowerbody, upperbody, backpackhead, weapon |
| LunarOps (black and red) | `.../Skins/LunarOps/Meshes/Wraith_LunarOps` | 5 | 94,253 | 47,809 | 5: adds an accessories slot |
| ODGreen (olive and black) | `.../Skins/ODGreen/Meshes/Wraith_ODGreen` | 5 | 93,602 | 47,774 | 6: adds scalloped attachments |

- **Skeleton:** all three skins share `Wraith_Skeleton` (Paragon hero skeleton, "scaled to Epic skeleton" per the listing).
  Retargeting to Shieldbreak's mannequin needs an IK Retargeter.
- **The gun is part of the character mesh** (slot `M03_weapon`). Hiding or replacing it means masking that material slot or splitting the mesh.
- **Animation:**
  - 229 AnimSequences, 5 montages, 8 blend spaces, 2 1D blend spaces and 3 aim offsets.
  - One AnimBlueprint: `Wraith_AnimBlueprint`.
- **Materials:**
  - 74 Materials and 71 MaterialFunctions, using Paragon's layered material system (metal, painted, rubber and synthetic layers).
  - Only 16 MaterialInstanceConstants, all of them FX.
- **Textures:** 201 in total. 52 are 4096x4096, 44 are 2048, 35 are 1024, and the rest are smaller. These include shared 4K material-layer textures,
  for example `T_AnodizedMetal_D/N` and `T_Wraith_Body_Color_Overlay`.
- **Other content:** 44 particle systems, 152 sound cues, 362 dialogue waves, 37 static meshes (FX), 2 maps and 2 physics assets.

Full dump: `inventory.json`.

## Previews

`wraith_<skin>_{front34,back,helmet}.jpg` and `wraith_sheet.jpg` (rows: default, LunarOps, ODGreen). They're editor high-res shots at
1600x900: a temporary level with a directional light, sky atmosphere, sky light and two rect lights, auto exposure, and the idle
animation at 0.5 s. Full-size PNGs are in `C:\Users\addve\Documents\wraith_preview\`.

**Read:** this is a sleek full-helmet armoured soldier with crisp hard-surface paneling and strong material layering. It's far above the
AI-generated suits in detail.
- LunarOps (black and red) reads closest to a team-coloured trooper. ODGreen is the most military.
- The silhouette is slim and agile, not bulky Halo-heavy.
- Team colour would come from Paragon's layered material parameters, not from our `MI_SB_Soldier`.
