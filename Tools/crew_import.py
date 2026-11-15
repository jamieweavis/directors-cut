"""Director's Cut: import the crew body (Art/Crew/*.fbx, rigged, no animation) into /Game/DC/Characters/Crew.

Headless (materials need a real RHI, so no -nullrhi):
  lockf /private/tmp/claude-501/dc_ue_run.lock "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/DirectorsCut.uproject" -run=pythonscript -script="$PWD/Tools/crew_import.py" -unattended \
    -nosplash -AllowCommandletRendering -abslog="$PWD/Saved/Logs/crew_import.log" \
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False'

Creates, replacing anything already there, for each body in BODIES:
  SK_Crew_<Name> (+ its skeleton / physics asset)   the body; animated at runtime by copying the mannequin's pose
  T_Crew_<Name>_BaseColor (+ _Normal if present)    the baked textures
  MI_Crew_<Name>                                    M_DC_CrewSkin with that body's textures
and once:
  M_DC_CrewSkin                                     lit textured material. Params: BaseTex, NormalTex (flat by default),
                                                    Tint (vec, multiplies the texture), Roughness,
                                                    AccentColor (vec) + RimStrength (fresnel rim, type colour)
Logs the ref-pose bounds (the pawn logs the bone hierarchy the first time it builds the body).
"""
import os

import unreal

DEST = "/Game/DC/Characters/Crew"
MAT_DIR = "/Game/DC/Materials"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

# (asset name, fbx, basecolor jpg, normal jpg or None), all relative to Art/Crew. Order matches DCPaths::CrewBodies.
BODIES = [
    ("Hooded", "Crew_Hooded.fbx", "Crew_Hooded.fbm/T_Crew_Hooded_BaseColor.jpg", None),
    ("Criminal", "Crew_Criminal.fbx", "Crew_Criminal.fbm/T_Crew_Criminal_basecolor.jpg",
     "Crew_Criminal.fbm/T_Crew_Criminal_normal.jpg"),
    ("Hoodie", "Crew_Hoodie.fbx", "Crew_Hoodie.fbm/T_Crew_Hoodie_basecolor.jpg",
     "Crew_Hoodie.fbm/T_Crew_Hoodie_normal.jpg"),
    ("LeatherJacket", "Crew_LeatherJacket.fbx", "Crew_LeatherJacket.fbm/T_Crew_LeatherJacket_basecolor.jpg",
     "Crew_LeatherJacket.fbm/T_Crew_LeatherJacket_normal.jpg"),
    ("Tracksuit", "Crew_Tracksuit.fbx", "Crew_Tracksuit.fbm/T_Crew_Tracksuit_basecolor.jpg",
     "Crew_Tracksuit.fbm/T_Crew_Tracksuit_normal.jpg"),
    ("Casual", "Crew_Casual.fbx", "Crew_Casual.fbm/T_Crew_Casual_basecolor.jpg",
     "Crew_Casual.fbm/T_Crew_Casual_normal.jpg"),
]


def log(msg):
    unreal.log("[DCCrewImport] " + msg)


def import_file(path, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", False)
    if options is not None:
        t.set_editor_property("options", options)
    AT.import_asset_tasks([t])
    return list(t.get_editor_property("imported_object_paths") or [])


def make_skin_material():
    path = MAT_DIR + "/M_DC_CrewSkin"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset("M_DC_CrewSkin", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    MP = unreal.MaterialProperty

    ts = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    ts.set_editor_property("parameter_name", "BaseTex")
    ts.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, 250)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 100)
    MEL.connect_material_expressions(ts, "RGB", mul, "A")
    MEL.connect_material_expressions(tint, "", mul, "B")
    MEL.connect_material_property(mul, "", MP.MP_BASE_COLOR)

    rough = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 300)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.8)
    MEL.connect_material_property(rough, "", MP.MP_ROUGHNESS)

    nt = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -300)
    nt.set_editor_property("parameter_name", "NormalTex")
    nt.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    nt.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
    MEL.connect_material_property(nt, "RGB", MP.MP_NORMAL)

    # Fresnel rim in the crew member's type colour (same idea as M_DC_Criminal).
    acc = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, 450)
    acc.set_editor_property("parameter_name", "AccentColor")
    acc.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 1))
    fr = MEL.create_material_expression(m, unreal.MaterialExpressionFresnel, -700, 600)
    fr.set_editor_property("exponent", 3.0)
    rs = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 700)
    rs.set_editor_property("parameter_name", "RimStrength")
    rs.set_editor_property("default_value", 0.6)
    m1 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 500)
    MEL.connect_material_expressions(acc, "", m1, "A")
    MEL.connect_material_expressions(fr, "", m1, "B")
    m2 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 550)
    MEL.connect_material_expressions(m1, "", m2, "A")
    MEL.connect_material_expressions(rs, "", m2, "B")
    MEL.connect_material_property(m2, "", MP.MP_EMISSIVE_COLOR)

    m.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    return m


def import_body(art, name, fbx, base_jpg, normal_jpg, skin):
    tex_paths = import_file(os.path.join(art, base_jpg), "T_Crew_%s_BaseColor" % name)
    tex = unreal.load_asset(tex_paths[0]) if tex_paths else None
    ntex = None
    if normal_jpg:
        n_paths = import_file(os.path.join(art, normal_jpg), "T_Crew_%s_Normal" % name)
        ntex = unreal.load_asset(n_paths[0]) if n_paths else None
        if ntex:
            ntex.set_editor_property("srgb", False)
            ntex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    log("%s textures: %s %s" % (name, tex_paths, ntex))

    before = set(EAL.list_assets(DEST, recursive=True, include_folder=False))
    mesh_paths = import_file(os.path.join(art, fbx), "SK_Crew_" + name)
    log("%s fbx imported: %s" % (name, mesh_paths))
    mesh = None
    for p in mesh_paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
    # Drop whatever materials/textures the importer generated on its own; M_DC_CrewSkin replaces them.
    for p in set(EAL.list_assets(DEST, recursive=True, include_folder=False)) - before:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.MaterialInterface) or isinstance(a, unreal.Texture):
            log("removing importer extra: %s" % p)
            EAL.delete_asset(p.split(".")[0])
    if mesh is None:
        log("ERROR: no skeletal mesh came out of %s" % fbx)
        return

    mi = AT.create_asset("MI_Crew_" + name, DEST, unreal.MaterialInstanceConstant,
                         unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, skin)
    if tex:
        MEL.set_material_instance_texture_parameter_value(mi, "BaseTex", tex)
    if ntex:
        MEL.set_material_instance_texture_parameter_value(mi, "NormalTex", ntex)
    mats = mesh.get_editor_property("materials")
    for sm in mats:
        sm.set_editor_property("material_interface", mi)
    mesh.set_editor_property("materials", mats)
    log("%s material slots: %d -> %s" % (name, len(mats), mi.get_name()))

    b = mesh.get_bounds()
    log("%s bounds origin=%s extent=%s" % (name, b.origin, b.box_extent))
    log("%s skeleton: %s" % (name, mesh.get_editor_property("skeleton").get_path_name()))


def main():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    art = os.path.join(proj, "Art", "Crew")

    # Start clean so stale imports (old material slots, renamed textures) don't linger.
    if EAL.does_directory_exist(DEST):
        EAL.delete_directory(DEST)

    skin = make_skin_material()
    for body in BODIES:
        import_body(art, *body, skin=skin)

    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        a = unreal.load_asset(p)
        if a:
            EAL.save_loaded_asset(a, False)
    log("assets: %s" % EAL.list_assets(DEST, recursive=True, include_folder=False))


main()
