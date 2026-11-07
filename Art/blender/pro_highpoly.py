"""High-poly detail layer for the professional bake (bake_weapons.py, guns in PRO): the detail a weapon artist models
into the high poly and bakes down, instead of carrying it as game geometry.

HIGH gets:
 - real panel-line grooves cut by booleans wherever build_weapons.py drew a seam strip;
 - counterbored recesses round every screw head;
 - through-cut slots where the low poly only had dark slabs (cooling slots, brake ports, M-LOK slots, stock holes);
 - extra machined pockets and grooves (CUTS) on the receiver, shroud, stock and magazine;
 - a second fine bevel on every boolean edge, then weighted normals (face area, keep sharp) for clean shading.
LOW drops the seam strips, screws and fake slot slabs: the bake carries them, so the game mesh gets lighter.

All shapes are original (Shieldbreak's own Vanguard BR).
"""
import math

import bpy
import bmesh
from mathutils import Matrix, Vector

import sb_kit as k
import build_weapons as bw

# Thin dark slabs in build_weapons.py that stand in for slots: cut through for real on HIGH, dropped from LOW.
SLOT_PREFIXES = ("cool_slot", "side_slot", "brake_top", "brake_low", "mlok", "ch_channel", "adj_hole", "muzzle_port",
                 "vent_window", "seam")
# Small parts the cutters must not touch (they sit in the recesses).
NO_CUT = ("screw", "screw_hex", "tooth", "trigger", "pin", "witness", "rib", "knurl", "turret", "battery", "cut_")


class Collector:
    """Records cutters made while a gun is built, and whether we are building HIGH or LOW."""

    def __init__(self, high):
        self.high = high
        self.cutters = []

    def cutter_box(self, center, size, rot_x=0.0):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
        mesh = bpy.data.meshes.new("cut")
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new("cut_box", mesh)
        bpy.context.scene.collection.objects.link(obj)
        obj.rotation_euler = (rot_x, 0, 0)
        obj.location = center
        obj.hide_render = True
        self.cutters.append(obj)
        return obj

    def cutter_cyl(self, center, radius, length, axis="X", verts=24):
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=radius, radius2=radius, depth=length)
        rot = {"X": Matrix.Rotation(math.radians(90), 4, "Y"), "Y": Matrix.Rotation(math.radians(-90), 4, "X"),
               "Z": Matrix.Identity(4)}[axis]
        bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(center)) @ rot, verts=bm.verts)
        mesh = bpy.data.meshes.new("cut")
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new("cut_cyl", mesh)
        bpy.context.scene.collection.objects.link(obj)
        obj.hide_render = True
        self.cutters.append(obj)
        return obj


def patched(collector):
    """Context manager: swaps build_weapons' seam() and screw() for HIGH/LOW versions."""
    orig_seam, orig_screw = bw.seam, bw.screw

    def seam(p, x, a, b, w=0.07, mat="Polymer"):
        if not collector.high:
            return
        (y0, z0), (y1, z1) = a, b
        length = math.hypot(y1 - y0, z1 - z0)
        # A V-ish groove: 0.12 cm wide, cut 0.12 cm into the face (the strip sat 0.02 proud of it).
        collector.cutter_box((x, (y0 + y1) * 0.5, (z0 + z1) * 0.5), (0.3, length, max(0.12, w * 1.4)),
                             rot_x=math.atan2(z1 - z0, y1 - y0))

    def screw(p, x, y, z, r=0.32, mat="Steel"):
        if not collector.high:
            return
        orig_screw(p, x, y, z, r, mat)
        out = 1 if x > 0 else -1
        collector.cutter_cyl((x - out * 0.02, y, z), r * 1.45, 0.34, "X", 24)

    class _Ctx:
        def __enter__(self):
            bw.seam, bw.screw = seam, screw

        def __exit__(self, *exc):
            bw.seam, bw.screw = orig_seam, orig_screw

    return _Ctx()


def is_slot(obj):
    base = obj.name.split(".")[0]
    return any(base.startswith(prefix) for prefix in SLOT_PREFIXES)


def slot_to_cutter(collector, obj):
    """A thin slab becomes a cutter 0.5 cm deep along its thinnest axis (centred on the slab)."""
    dims = obj.dimensions.copy()
    axis = min(range(3), key=lambda i: dims[i])
    scale = [1.0, 1.0, 1.0]
    scale[axis] = max(1.0, 0.5 / max(dims[axis], 1e-3))
    for mod in list(obj.modifiers):
        obj.modifiers.remove(mod)
    obj.scale = Vector(scale)
    obj.name = "cut_slot"
    obj.hide_render = True
    collector.cutters.append(obj)


def extra_cuts(collector):
    """Machined detail the old model never had (cm, mesh space). Mirrored on both sides unless noted."""
    c = collector
    for s in (1, -1):
        # Stock: two lightening pockets in the lower arm and a groove along the upper arm.
        c.cutter_box((s * 1.95, -24.5, -1.6), (0.6, 8.0, 1.4))
        c.cutter_box((s * 1.95, -31.2, -3.0), (0.6, 3.2, 1.2), rot_x=math.radians(-20))
        c.cutter_box((s * 2.0, -27.0, 5.75), (0.5, 15.0, 0.14), rot_x=math.radians(5))
        # Lower receiver: a long relief groove and a stepped pocket above the grip.
        c.cutter_box((s * 2.3, 4.0, -0.6), (0.4, 26.0, 0.14))
        c.cutter_box((s * 2.3, -7.5, -2.4), (0.36, 5.0, 1.2))
        # Grip: three finger-ridge grooves wrapping the front strap.
        for i, z in enumerate((-8.0, -10.6, -13.2)):
            c.cutter_box((0.0, 2.2 - i * 0.25, z), (4.0, 1.2, 0.18), rot_x=math.radians(-8))
        # Magazine: a stepped panel on each side.
        c.cutter_box((s * 1.95, 15.4, -10.0), (0.3, 4.2, 6.0), rot_x=math.radians(-11))
        # Shroud chin and nose: chamfer grooves following the swept lines.
        c.cutter_box((s * 2.9, 44.0, -0.9), (0.5, 9.0, 0.14), rot_x=math.radians(23))
        # Receiver upper (between the ceramic panels and the top layer): a machined step.
        c.cutter_box((s * 2.6, 2.0, 7.4), (0.4, 24.0, 0.16))
        # Scope tube: two ring grooves either side of the clamps.
        for y in (6.0, 18.0):
            c.cutter_cyl((0.0, y, 14.4), 1.62, 0.12, "Y", 48)
    # Top of the receiver: a recessed channel round the ammo counter bezel.
    c.cutter_box((0.0, -9.5, 9.2), (4.2, 4.6, 0.25))
    # Butt pad: horizontal grip grooves.
    for i in range(6):
        c.cutter_box((0.0, -39.1, -5.6 + i * 1.8), (5.0, 0.4, 0.22))


def finish_high(parts, collector):
    """Booleans (only cutters touching each part), a fine second bevel and weighted normals on every HIGH part."""
    cutters = collector.cutters
    bpy.context.view_layer.update()

    def bbox(obj):
        pts = [obj.matrix_world @ Vector(cn) for cn in obj.bound_box]
        return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
                Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))

    cut_boxes = [(c, bbox(c)) for c in cutters]
    cuts = 0
    for obj in parts:
        if obj.type != "MESH":
            continue
        for poly in obj.data.polygons:
            poly.use_smooth = True
        base = obj.name.split(".")[0]
        dims = obj.dimensions
        target = max(dims) > 1.5 and not any(base.startswith(n) for n in NO_CUT)
        hits = []
        if target:
            lo, hi = bbox(obj)
            for c, (clo, chi) in cut_boxes:
                if all(clo[i] <= hi[i] and chi[i] >= lo[i] for i in range(3)):
                    hits.append(c)
        if hits:
            coll = bpy.data.collections.new("cuts_" + obj.name)
            for c in hits:
                coll.objects.link(c)
            mod = obj.modifiers.new("Cuts", "BOOLEAN")
            mod.operation = "DIFFERENCE"
            mod.operand_type = "COLLECTION"
            mod.collection = coll
            mod.solver = "EXACT"
            bev = obj.modifiers.new("FineBevel", "BEVEL")
            bev.width = 0.035
            bev.segments = 2
            bev.limit_method = "ANGLE"
            bev.angle_limit = math.radians(40)
            bev.harden_normals = True
            bev.use_clamp_overlap = True
            cuts += len(hits)
        wn = obj.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
        wn.mode = "FACE_AREA"
        wn.keep_sharp = True
        wn.weight = 70
    return cuts


def remove_cutters(collector):
    for c in collector.cutters:
        if c.name in bpy.data.objects:
            bpy.data.objects.remove(c, do_unlink=True)
    for coll in list(bpy.data.collections):
        if coll.name.startswith("cuts_"):
            bpy.data.collections.remove(coll)
    collector.cutters = []
