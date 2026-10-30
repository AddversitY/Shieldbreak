"""Converts the chosen pieces of Quaternius' Modular Sci-Fi MegaKit (CC0, free Standard edition) to Shieldbreak's mesh
conventions (run headless: python3 Art/blender/build_scifi_kit.py <kit folder> [--render=<folder>]).

<kit folder> is the unzipped "Modular SciFi MegaKit[Standard]" folder (Saved/Quaternius/kit on the desktop, fetched
by Tools/GetSciFiKit.ps1). The game uses the pieces to clad the greybox blocks inside the maps (visual only, no
collision): see AShieldbreakArena::BuildCladding and docs/MAP_ART_STYLE.md.

Mesh space: 1 unit = 1 cm, Unreal axes (sb_kit.export_fbx converts).
  Wall pieces  the panel's face on X = 0 looking +X, centred on Y (400 cm wide), base at Z = 0.
  Floor pieces the walking surface on Z = 0 looking +Z, centred on X and Y (400 x 400 cm).
  Column / prop pieces  base centred on the origin at Z = 0.
Material slots are renamed to the trim sheet they sample (M_Trim01, M_Trim02, M_Trim03, M_Trim03Dark, M_KitLight,
M_KitDark); Tools/import_scifi_kit.py gives each its material.

Exported to Art/Export/SciFiKit (SM_Q_<piece>.fbx) with scifikit_report.txt for the import's size checks.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)

import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Export", "SciFiKit")

# kit path (under glTF/) -> (our name, kind)
PIECES = {
    "Walls/ShortWall_Metal2_Straight": ("WallMetal", "wall"),
    "Walls/ShortWall_WhitePlate2_Straight": ("WallPlate", "wall"),
    "Walls/ShortWall_DarkMetal2_Straight": ("WallDark", "wall"),
    "Walls/ShortWall_Band2_Straight": ("WallBand", "wall"),
    "Walls/ShortWall_MetalPlates_Straight": ("BandPlates", "wall"),
    "Walls/ShortWall_Triangles_Straight": ("BandTriangles", "wall"),
    "Walls/ShortWall_AccentStrip_Straight": ("BandAccent", "wall"),
    "Walls/ShortWall_DarkPlastic_Straight": ("BandDark", "wall"),
    "Walls/ShortWall_Simple1_Straight": ("BandSimple", "wall"),
    "Walls/WallAstra_Straight_Flat": ("PanelAstra", "wall"),
    "Walls/WallBand_Straight": ("PanelBand", "wall"),
    "Walls/TopSimple_Straight": ("PanelTop", "wall"),
    "Walls/TopAstra_Straight": ("PanelTopAstra", "wall"),
    "Platforms/Platform_Metal": ("FloorMetal", "floor"),
    "Platforms/Platform_Metal2": ("FloorMetal2", "floor"),
    "Platforms/Platform_Squares": ("FloorSquares", "floor"),
    "Platforms/Platform_DarkPlates": ("FloorDark", "floor"),
    "Platforms/Platform_CenterPlate": ("FloorCenter", "floor"),
    "Platforms/Platform_3Plates": ("FloorPlates", "floor"),
    "Platforms/Platform_X": ("FloorX", "floor"),
    "Platforms/Platform_Simple": ("FloorSimple", "floor"),
    "Columns/Column_Hollow": ("ColumnHollow", "column"),
    "Columns/Column_Round": ("ColumnRound", "column"),
    "Columns/Column_Pipes": ("ColumnPipes", "column"),
    "Props/Prop_Crate4": ("Crate", "column"),
    "Props/Prop_Barrel_Large": ("Barrel", "column"),
    "Props/Prop_Vent_Big": ("Vent", "column"),
}


def slot_name(material):
    """Kit material -> our slot: the trim sheet it samples, or a light / dark solid."""
    name = material.name if material else ""
    if name.startswith("MI_Trim_03_Dark"):
        return "M_Trim03Dark"
    for sheet in ("01", "02", "03"):
        if name.startswith("MI_Trim_" + sheet):
            return "M_Trim" + sheet
    if name.startswith("M_Light"):
        return "M_KitLight"
    return "M_KitDark"


def load_piece(kit, path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(kit, "glTF", path + ".gltf"))
    meshes = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        world = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = world
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    for other in [o for o in bpy.data.objects if o not in before and o != obj]:
        bpy.data.objects.remove(other)
    # The importer honours the scene's centimetre unit scale, so only the object transform needs baking in.
    obj.data.transform(obj.matrix_world)
    obj.matrix_world = Matrix.Identity(4)
    return obj


def facing(obj):
    """Area-weighted average face normal."""
    total = Vector((0, 0, 0))
    for poly in obj.data.polygons:
        total += poly.normal * poly.area
    return total


def place(obj, kind):
    verts = obj.data.vertices
    if kind == "wall":
        # Turn the panel so its face looks +X, then put that face on X = 0.
        if facing(obj).x < 0:
            obj.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
        xs = [v.co.x for v in verts]
        ys = [v.co.y for v in verts]
        zs = [v.co.z for v in verts]
        obj.data.transform(Matrix.Translation((-max(xs), -(min(ys) + max(ys)) / 2, -min(zs))))
    elif kind == "floor":
        if facing(obj).z < 0:
            obj.data.transform(Matrix.Rotation(math.pi, 4, "X"))
        xs = [v.co.x for v in verts]
        ys = [v.co.y for v in verts]
        zs = [v.co.z for v in verts]
        obj.data.transform(Matrix.Translation((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -max(zs))))
    else:
        xs = [v.co.x for v in verts]
        ys = [v.co.y for v in verts]
        zs = [v.co.z for v in verts]
        obj.data.transform(Matrix.Translation((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -min(zs))))


def rename_slots(obj):
    """One slot per target material, in a stable order, with the faces moved over."""
    targets = []
    remap = {}
    for index, material in enumerate(obj.data.materials):
        target = slot_name(material)
        if target not in targets:
            targets.append(target)
        remap[index] = targets.index(target)
    for poly in obj.data.polygons:
        poly.material_index = remap.get(poly.material_index, 0)
    obj.data.materials.clear()
    for target in targets:
        material = bpy.data.materials.get(target) or bpy.data.materials.new(target)
        obj.data.materials.append(material)
    return targets


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    kit = args[-1] if args else os.environ.get("SB_SCIFI_KIT", "")
    if not os.path.isdir(os.path.join(kit, "glTF")):
        raise SystemExit("usage: build_scifi_kit.py <Modular SciFi MegaKit[Standard] folder>")
    os.makedirs(OUT, exist_ok=True)
    report = []
    built = []
    for path, (name, kind) in PIECES.items():
        sb_kit.reset_scene()
        obj = load_piece(kit, path)
        obj.name = "SM_Q_" + name
        obj.data.name = obj.name
        place(obj, kind)
        slots = rename_slots(obj)
        line = "%s: %s, tris %d, %s, slots %s" % (obj.name, kind, sb_kit.triangle_count(obj), sb_kit.bounds(obj), ",".join(slots))
        print(line)
        report.append(line)
        sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
        built.append((obj.name, kind))
    with open(os.path.join(OUT, "scifikit_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")
    previews = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None)
    if previews:
        render_sheet(kit, previews)


def render_sheet(kit, folder):
    """Front views of the walls and top views of the floors, with the kit's own textures, for picking pieces."""
    os.makedirs(folder, exist_ok=True)
    for kind, view in (("wall", "front"), ("floor", "top")):
        sb_kit.reset_scene()
        scene = bpy.context.scene
        names = [(p, n) for p, (n, k) in PIECES.items() if k == kind]
        for i, (path, name) in enumerate(names):
            obj = load_piece(kit, path)
            place(obj, kind)
            col, row = i % 5, i // 5
            if kind == "wall":
                obj.location = (0, -col * 450.0, -row * 380.0)
            else:
                obj.location = (col * 450.0, -row * 450.0, 0)
        cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
        scene.collection.objects.link(cam)
        cam.data.type = "ORTHO"
        rows = (len(names) + 4) // 5
        if kind == "wall":
            cam.location = (3000, -900, -(rows - 1) * 190 + 150)
            cam.rotation_euler = (math.radians(90), 0, math.radians(90))
            cam.data.ortho_scale = 2400
        else:
            cam.location = (900, -(rows - 1) * 225, 3000)
            cam.rotation_euler = (0, 0, 0)
            cam.data.ortho_scale = 2400
        cam.data.clip_end = 10000
        scene.camera = cam
        sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
        sun.data.energy = 3.0
        sun.rotation_euler = (math.radians(40), math.radians(20), math.radians(70))
        scene.collection.objects.link(sun)
        world = bpy.data.worlds.new("w")
        world.use_nodes = True
        world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.65, 0.7, 1)
        scene.world = world
        scene.render.engine = "CYCLES"
        scene.cycles.samples = 16
        scene.render.resolution_x = 1600
        scene.render.resolution_y = 900
        scene.render.filepath = os.path.join(folder, "scifikit_%s.png" % kind)
        bpy.ops.render.render(write_still=True)


main()
