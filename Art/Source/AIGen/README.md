# AI-generated soldier_1 (prototype source)

| File | What it is |
|---|---|
| `concept_1.png` | Concept image, 768x1344, FLUX.1-schnell (Hugging Face Space `black-forest-labs/FLUX.1-schnell`), seed 1201, 4 steps |
| `soldier_1_shape.glb` | Untextured shape, 450,266 triangles, straight from Hunyuan3D-2 |
| `soldier_1_hunyuan2local.glb` | Textured mesh, 80,000 triangles, one 2048x2048 base-colour texture (lighting partly baked in) |
| `soldier_1_preview.png` | Front, three-quarter, side and back renders of the textured GLB |

## How it was made

Hunyuan3D-2 (`github.com/Tencent-Hunyuan/Hunyuan3D-2`, weights `tencent/Hunyuan3D-2`) was run
locally on an RTX 2080 Ti (11 GB) with Python 3.11, PyTorch 2.5.1+cu124, transformers 4.46.3 and
diffusers 0.31.0. The texture extensions were built against a user-local CUDA 12.4 (TORCH_CUDA_ARCH_LIST=7.5).

- Background removal: `hy3dgen.rembg.BackgroundRemover` on `concept_1.png`
- Shape: `hunyuan3d-dit-v2-0`, 50 steps, guidance 5.0, octree resolution 384, num_chunks 8000, seed 1234;
  FloaterRemover and DegenerateFaceRemover (278 s)
- Texture: FaceReducer to 80,000 faces, then `hunyuan3d-paint-v2-0-turbo` with default views
  (6 views, render and texture size 2048) and model CPU offload (118 s)

Known gaps: there's no back power pack (the concept is front only), only base colour is
included (no normal, roughness or metal maps), the mesh is an unstructured triangle soup, and
there's no skeleton.

## v2: multi-view pass, adds the back power pack

| File | What it is |
|---|---|
| `soldier_1_v2_mv.glb` | **Current best.** Textured, 80,000 triangles, 2048 base colour. Has the back power pack (ribbed cylinders, hoses). |
| `soldier_1_v2_mv_shape.glb` | Untextured multi-view shape, 494,380 triangles |
| `soldier_1_v2_preview.png` | Front, three-quarter, side and back renders of v2 |
| `soldier_1_v2_views/{left,back,right}.png` | Guide views the shape was built from |

How v2 was made:
1. Rendered orthographic left, back and right views (768x1344) of the v1 textured mesh, with a
   blocked-in power pack built from primitives (a housing, two ribbed vertical cylinders and hoses to the shoulders).
2. Refined those renders with SDXL base 1.0 img2img (fp16-fix VAE): strength 0.45, 40 steps,
   guidance 6.5, seed 7. The prompt describes the same soldier per view, adding the power pack.
3. Hunyuan3D-2mv (`hunyuan3d-dit-v2-mv`, fp16): front = background-removed `concept_1.png`,
   plus the left, back and right views. 50 steps, octree resolution 384, num_chunks 20000, seed 1234 (488 s).
4. FaceReducer to 80k, then `hunyuan3d-paint-v2-0-turbo` conditioned on the **front concept only** (123 s).
   Conditioning on all four views was also tried, but it washed out the colours and dropped the respirator detail.

Known v2 gaps: the respirator jaw is flatter in the geometry than in v1 (the texture still paints
it), the power pack is boxy, and there's still only base colour and no skeleton.

## v3: higher-resolution multi-view pass

| File | What it is |
|---|---|
| `soldier_1_v3_mv.glb` | Textured, 120,000 triangles, **4096** base colour (12.5 MB) |
| `soldier_1_v3_mv_shape.glb` | Untextured, 882,512 triangles (15.9 MB) |
| `soldier_1_v3_preview.png` | Front, three-quarter, side and back renders |

The inputs are the same four views as v2 (`soldier_1_v2_views/` plus the front concept). Hunyuan3D-2mv ran at
**octree resolution 512** and **60 steps** (num_chunks 20000, seed 1234, 299 s, 6.6 GB peak VRAM on the
2080 Ti). The mesh was reduced to 120k faces, then painted with `hunyuan3d-paint-v2-0-turbo` from the front concept at a
4096 texture size (297 s). Compared with v2, plate borders and pouch edges are a little crisper and the
pack ribs are cleaner, but it's still one fused surface with the same overall forms.

## Licence: prototype only

Hunyuan3D-2 is under the Tencent Hunyuan 3D 2.0 Community License: **non-commercial**, and it
**does not apply in the EU, UK or South Korea**. Use these files only for prototyping and
reference. Don't ship them, or meshes derived from them, in a commercial build.
