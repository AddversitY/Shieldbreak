"""Read-only Manny slot/parent scalar and vector defaults for the desktop handoff.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> -nullrhi.
Does not change or save any Unreal asset.
"""
from pathlib import Path

import unreal

MANNY = "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"
mel = unreal.MaterialEditingLibrary
lines = [
    "Manny material scalar/vector parameters (asset defaults, not runtime dynamic instances)",
    "Mesh: " + MANNY,
    "Instance values below are effective inherited asset defaults; local overrides are also listed.",
    "Master values are authored defaults. Named lookups use GlobalParameter; override records include association/index.",
    "No assets modified or saved.",
]


def value_text(value):
    if isinstance(value, unreal.LinearColor):
        return "(r=%.9g, g=%.9g, b=%.9g, a=%.9g)" % (value.r, value.g, value.b, value.a)
    return "%.9g" % value


def main():
    global lines
    mesh = unreal.load_asset(MANNY)
    if mesh is None:
        raise RuntimeError("Manny mesh is missing")
    slots = mesh.get_editor_property("materials")
    if not slots:
        raise RuntimeError("Manny has no material slots")
    for index, slot in enumerate(slots):
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("Manny slot %d has no material" % index)
        lines += ["", "SLOT %d: %s" % (index, slot.get_editor_property("material_slot_name"))]
        visited = set()
        depth = 0
        while material is not None:
            path = material.get_path_name()
            if path in visited:
                raise RuntimeError("Material parent cycle: " + path)
            visited.add(path)
            instance = isinstance(material, unreal.MaterialInstanceConstant)
            if not instance and not isinstance(material, unreal.Material):
                raise RuntimeError("Unsupported material class: " + material.get_class().get_name())
            lines += ["  %s %s [%s]" % ("MATERIAL" if depth == 0 else "PARENT", path, material.get_class().get_name())]
            for kind in ("scalar", "vector"):
                names = sorted(set(str(n) for n in getattr(mel, "get_%s_parameter_names" % kind)(material)))
                lines.append("    %s parameters: %d" % (kind, len(names)))
                getter = getattr(mel, ("get_material_instance_%s_parameter_value" if instance else
                                       "get_material_default_%s_parameter_value") % kind)
                for name in names:
                    lines.append("      %s = %s" % (name, value_text(getter(material, name))))
                if instance:
                    overrides = material.get_editor_property(kind + "_parameter_values")
                    lines.append("    local %s overrides: %d" % (kind, len(overrides)))
                    for override in overrides:
                        info = override.get_editor_property("parameter_info")
                        lines.append("      %s [association=%s, index=%s] = %s" % (
                            info.get_editor_property("name"), info.get_editor_property("association"),
                            info.get_editor_property("index"), value_text(override.get_editor_property("parameter_value"))))
            material = material.get_editor_property("parent") if instance else None
            depth += 1
    
    out = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / (
        "docs/playtests/2026-11-04_character_pass/reports/mannequin_material_params.txt")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    unreal.log("Manny material parameter report written: %s (%d slots)" % (out, len(slots)))


main()
# Release transient Unreal struct wrappers before engine/Python shutdown.
import gc
gc.collect()
