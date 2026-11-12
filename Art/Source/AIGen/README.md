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

## Licence: prototype only

Hunyuan3D-2 is under the Tencent Hunyuan 3D 2.0 Community License: **non-commercial**, and it
**does not apply in the EU, UK or South Korea**. Use these files only for prototyping and
reference. Don't ship them, or meshes derived from them, in a commercial build.
