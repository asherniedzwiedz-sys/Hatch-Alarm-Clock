#!/usr/bin/env python3
"""Pack the exported STLs into one Bambu Studio project with a plate per filament run:
    python make_3mf.py        (after build.py)  ->  sunrise_clock_A1.3mf
Settings are Bambu Studio's own "0.20mm Standard @BBL A1" with two Bambu PLA Basic slots
(1 = white, 2 = black), taken unmodified from a project Bambu Studio saved."""
import json
import os
import uuid
import zipfile
from pathlib import Path

import numpy as np

from build import OUT, load_stl, manifold_errors

HERE = Path(os.path.relpath(Path(__file__).parent))  # relative: some sandboxed Pythons can't open long absolute paths
SETTINGS = HERE / "bambu_a1_project_settings.json"
TARGET = HERE / "sunrise_clock_A1.3mf"
BED, MARGIN = 256.0, 6.0
GAP = BED * 1.2  # Bambu Studio lays plates out on a grid, 1.2 bed widths apart
WHITE, BLACK = 1, 2

# plate name, [(part, filament, centre x, centre y on the bed, per-object settings)]
PLATES = [
    ("White: shell, diffuser, knob", [
        ("shell", WHITE, 98, 180, {"wall_loops": "4"}),
        ("diffuser", WHITE, 91, 52.5, {"sparse_infill_density": "100%"}),
        ("knob", WHITE, 215, 40, {}),
    ]),
    ("White: back cover, light box, speaker lid", [
        ("back_cover", WHITE, 98, 180, {"wall_loops": "4"}),
        ("light_box", WHITE, 89.7, 45.5, {"wall_loops": "4"}),
        ("speaker_lid", WHITE, 215, 45, {}),
    ]),
    ("Black: frame", [
        ("frame", BLACK, 128, 128, {"wall_loops": "4"}),
    ]),
]
COLS = 2  # Bambu Studio's column count for 3 plates
NS = ('xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02" '
      'xmlns:BambuStudio="http://schemas.bambulab.com/package/2021" '
      'xmlns:p="http://schemas.microsoft.com/3dmanufacturing/production/2015/06" requiredextensions="p"')


def mesh(name):
    tris = load_stl(OUT / f"{name}.stl")
    assert manifold_errors(tris) is None, name
    verts, idx = np.unique(tris.reshape(-1, 3), axis=0, return_inverse=True)
    lo, hi = verts.min(0), verts.max(0)
    return verts - (lo + hi) / 2, idx.reshape(-1, 3), hi - lo  # centred, like Bambu Studio stores objects


def object_file(oid, verts, faces):
    v = "\n".join(f'     <vertex x="{x:.6f}" y="{y:.6f}" z="{z:.6f}"/>' for x, y, z in verts)
    t = "\n".join(f'     <triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in faces)
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" {NS}>\n'
            f' <metadata name="BambuStudio:3mfVersion">1</metadata>\n <resources>\n'
            f'  <object id="{oid}" p:UUID="{uuid.uuid4()}" type="model">\n   <mesh>\n    <vertices>\n{v}\n    </vertices>\n'
            f'    <triangles>\n{t}\n    </triangles>\n   </mesh>\n  </object>\n </resources>\n <build/>\n</model>\n')


def main():
    settings = json.loads(SETTINGS.read_text(encoding="utf-8"))
    settings["filament_colour"] = ["#FFFFFF", "#000000"]
    files, objects, items, rels, cfg_objects, cfg_plates, assemble, seq = {}, [], [], [], [], [], [], {}
    k = 0
    for p, (plate_name, parts) in enumerate(PLATES):
        ox, oy = (p % COLS) * GAP, -(p // COLS) * GAP
        instances = []
        for name, filament, cx, cy, extra in parts:
            k += 1
            verts, faces, size = mesh(name)
            assert MARGIN <= cx - size[0] / 2 and cx + size[0] / 2 <= BED - MARGIN, (name, "x off the bed")
            assert MARGIN <= cy - size[1] / 2 and cy + size[1] / 2 <= BED - MARGIN, (name, "y off the bed")
            mesh_id, obj_id = 2 * k - 1, 2 * k
            path = f"3D/Objects/object_{k}.model"
            files[path] = object_file(mesh_id, verts, faces)
            rels.append(f' <Relationship Target="/{path}" Id="rel-{k}" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>')
            objects.append(f'  <object id="{obj_id}" p:UUID="{uuid.uuid4()}" type="model">\n   <components>\n'
                           f'    <component p:path="/{path}" objectid="{mesh_id}" p:UUID="{uuid.uuid4()}" transform="1 0 0 0 1 0 0 0 1 0 0 0"/>\n'
                           f'   </components>\n  </object>')
            xf = f"1 0 0 0 1 0 0 0 1 {ox + cx:.4f} {oy + cy:.4f} {size[2] / 2:.4f}"
            items.append(f'  <item objectid="{obj_id}" p:UUID="{uuid.uuid4()}" transform="{xf}" printable="1"/>')
            meta = "".join(f'\n    <metadata key="{key}" value="{val}"/>' for key, val in extra.items())
            cfg_objects.append(f'  <object id="{obj_id}">\n    <metadata key="name" value="{name}"/>\n'
                               f'    <metadata key="extruder" value="{filament}"/>{meta}\n    <metadata face_count="{len(faces)}"/>\n'
                               f'    <part id="{mesh_id}" subtype="normal_part">\n      <metadata key="name" value="{name}.stl"/>\n'
                               f'      <metadata key="matrix" value="1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"/>\n'
                               f'      <metadata key="source_file" value="{name}.stl"/>\n      <metadata key="extruder" value="{filament}"/>\n'
                               f'      <mesh_stat face_count="{len(faces)}" edges_fixed="0" degenerate_facets="0" facets_removed="0" facets_reversed="0" backwards_edges="0"/>\n'
                               f'    </part>\n  </object>')
            instances.append(f'    <model_instance>\n      <metadata key="object_id" value="{obj_id}"/>\n'
                             f'      <metadata key="instance_id" value="0"/>\n      <metadata key="identify_id" value="{100 + k}"/>\n    </model_instance>')
            assemble.append(f'   <assemble_item object_id="{obj_id}" instance_id="0" transform="{xf}" offset="0 0 0" />')
        cfg_plates.append(f'  <plate>\n    <metadata key="plater_id" value="{p + 1}"/>\n    <metadata key="plater_name" value="{plate_name}"/>\n'
                          f'    <metadata key="locked" value="false"/>\n' + "\n".join(instances) + "\n  </plate>")
        seq[f"plate_{p + 1}"] = {"sequence": []}

    files["[Content_Types].xml"] = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n'
                                    ' <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>\n'
                                    ' <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>\n'
                                    ' <Default Extension="png" ContentType="image/png"/>\n <Default Extension="gcode" ContentType="text/x.gcode"/>\n</Types>\n')
    files["_rels/.rels"] = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
                            ' <Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n</Relationships>\n')
    files["3D/_rels/3dmodel.model.rels"] = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
                                            + "\n".join(rels) + "\n</Relationships>\n")
    files["3D/3dmodel.model"] = (f'<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" {NS}>\n'
                                 f' <metadata name="Application">BambuStudio-{settings.get("version", "02.04.00.70")}</metadata>\n'
                                 ' <metadata name="BambuStudio:3mfVersion">1</metadata>\n <metadata name="Title">Sunrise Clock</metadata>\n'
                                 ' <resources>\n' + "\n".join(objects) + f'\n </resources>\n <build p:UUID="{uuid.uuid4()}">\n'
                                 + "\n".join(items) + "\n </build>\n</model>\n")
    files["Metadata/model_settings.config"] = ('<?xml version="1.0" encoding="UTF-8"?>\n<config>\n' + "\n".join(cfg_objects) + "\n"
                                               + "\n".join(cfg_plates) + "\n  <assemble>\n" + "\n".join(assemble) + "\n  </assemble>\n</config>\n")
    files["Metadata/project_settings.config"] = json.dumps(settings, indent=4)
    files["Metadata/slice_info.config"] = ('<?xml version="1.0" encoding="UTF-8"?>\n<config>\n  <header>\n'
                                           '    <header_item key="X-BBL-Client-Type" value="slicer"/>\n'
                                           f'    <header_item key="X-BBL-Client-Version" value="{settings.get("version", "02.04.00.70")}"/>\n  </header>\n</config>\n')
    files["Metadata/filament_sequence.json"] = json.dumps(seq)

    with zipfile.ZipFile(TARGET, "w", zipfile.ZIP_DEFLATED) as z:
        for name, text in files.items():
            z.writestr(name, text)
    print(f"wrote {TARGET.name}: {k} objects on {len(PLATES)} plates, {TARGET.stat().st_size / 1e6:.1f} MB")
    check()


def check():
    """Re-read the project: XML parses, meshes are manifold, every part is on its plate, none overlap."""
    import xml.etree.ElementTree as ET
    ns = {"m": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02",
          "p": "http://schemas.microsoft.com/3dmanufacturing/production/2015/06"}
    with zipfile.ZipFile(TARGET) as z:
        for n in z.namelist():
            if n.endswith((".model", ".config", ".rels", ".xml")) and n != "Metadata/project_settings.config":
                ET.fromstring(z.read(n))
        json.loads(z.read("Metadata/project_settings.config"))
        main_xml = ET.fromstring(z.read("3D/3dmodel.model"))
        cfg = ET.fromstring(z.read("Metadata/model_settings.config"))
        paths = {o.get("id"): o.find("m:components/m:component", ns).get("{%s}path" % ns["p"]).lstrip("/")
                 for o in main_xml.findall("m:resources/m:object", ns)}
        plate_of = {mi.find("metadata[@key='object_id']").get("value"): int(pl.find("metadata[@key='plater_id']").get("value")) - 1
                    for pl in cfg.findall("plate") for mi in pl.findall("model_instance")}
        boxes = {}
        for item in main_xml.findall("m:build/m:item", ns):
            oid = item.get("objectid")
            obj = ET.fromstring(z.read(paths[oid])).find("m:resources/m:object/m:mesh", ns)
            v = np.array([[float(e.get(a)) for a in "xyz"] for e in obj.find("m:vertices", ns)])
            f = np.array([[int(e.get(a)) for a in ("v1", "v2", "v3")] for e in obj.find("m:triangles", ns)])
            assert manifold_errors(v[f]) is None, f"{paths[oid]} not manifold"
            t = np.array(item.get("transform").split(), float)[9:]
            p = plate_of[oid]
            lo, hi = v.min(0) + t, v.max(0) + t
            lo[:2] -= [(p % COLS) * GAP, -(p // COLS) * GAP]
            hi[:2] -= [(p % COLS) * GAP, -(p // COLS) * GAP]
            assert lo[0] >= 0 and lo[1] >= 0 and hi[0] <= BED and hi[1] <= BED and abs(lo[2]) < 1e-3, (paths[oid], lo, hi)
            for other, (p2, lo2, hi2) in boxes.items():
                assert p2 != p or (hi[:2] <= lo2[:2]).any() or (hi2[:2] <= lo[:2]).any(), f"{oid} overlaps {other}"
            boxes[oid] = (p, lo, hi)
    print(f"check ok: {len(boxes)} manifold parts, all on their plates, no overlaps")


if __name__ == "__main__":
    main()
