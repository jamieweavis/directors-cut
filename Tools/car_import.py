"""Director's Cut: import the crew car models (Art/Cars/*.fbx, static, one fused mesh) into /Game/DC/Vehicles.

Headless (materials need a real RHI, so no -nullrhi):
  lockf /private/tmp/claude-501/dc_ue_run.lock "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/DirectorsCut.uproject" -run=pythonscript -script="$PWD/Tools/car_import.py" -unattended \
    -nosplash -AllowCommandletRendering -abslog="$PWD/Saved/Logs/car_import.log" \
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False'

Creates, replacing anything already there, for each car in CARS:
  SM_Car_<Name>                       the body (wheels, glass and interior are part of the one mesh)
  T_Car_<Name>_BaseColor / _Normal    the baked textures
  MI_Car_<Name>                       M_DC_CarSkin with that car's textures
and once:
  M_DC_CarSkin                        lit textured material. Params: BaseTex, NormalTex (flat by default),
                                      Tint (vec, multiplies the texture), Roughness, Metallic
Logs each mesh's bounds: ADCVehicle reads them at runtime to scale, centre and ground the model.
"""
import os

import unreal

DEST = "/Game/DC/Vehicles"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

# (asset name, fbx, basecolor jpg, normal jpg or None), relative to Art/Cars. Order matches DCPaths::CarModels.
CARS = [(n, "Car_%s.fbx" % n, "Car_%s.fbm/T_Car_%s_basecolor.jpg" % (n, n), "Car_%s.fbm/T_Car_%s_normal.jpg" % (n, n))
        for n in ("Estate", "Sedan", "Pickup", "Classic", "GreyEstate", "SUV", "Modern")]


def log(msg):
    unreal.log("[DCCarImport] " + msg)


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


def import_static_fbx(path, name):
    """Static FBX through the legacy importer: every node merged into one mesh, scene units converted to cm."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    sd = ui.static_mesh_import_data
    sd.set_editor_property("combine_meshes", True)
    sd.set_editor_property("convert_scene_unit", True)
    sd.set_editor_property("auto_generate_collision", False)
    sd.set_editor_property("generate_lightmap_u_vs", False)
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
    try:
        return import_file(path, name, ui)
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX True")


def make_material():
    path = DEST + "/M_DC_CarSkin"
    m = AT.create_asset("M_DC_CarSkin", DEST, unreal.Material, unreal.MaterialFactoryNew())
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
    rough.set_editor_property("default_value", 0.45)
    MEL.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    metal = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 400)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.2)
    MEL.connect_material_property(metal, "", MP.MP_METALLIC)

    nt = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -300)
    nt.set_editor_property("parameter_name", "NormalTex")
    nt.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    nt.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
    MEL.connect_material_property(nt, "RGB", MP.MP_NORMAL)

    MEL.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    log("material: %s" % path)
    return m


def import_car(art, name, fbx, base_jpg, normal_jpg, mat):
    tex_paths = import_file(os.path.join(art, base_jpg), "T_Car_%s_BaseColor" % name)
    tex = unreal.load_asset(tex_paths[0]) if tex_paths else None
    ntex = None
    if normal_jpg:
        n_paths = import_file(os.path.join(art, normal_jpg), "T_Car_%s_Normal" % name)
        ntex = unreal.load_asset(n_paths[0]) if n_paths else None
        if ntex:
            ntex.set_editor_property("srgb", False)
            ntex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    log("%s textures: %s %s" % (name, tex_paths, ntex))

    before = set(EAL.list_assets(DEST, recursive=True, include_folder=False))
    mesh_paths = import_static_fbx(os.path.join(art, fbx), "SM_Car_" + name)
    log("%s fbx imported: %s" % (name, mesh_paths))
    mesh = None
    for p in mesh_paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            mesh = a
    for p in set(EAL.list_assets(DEST, recursive=True, include_folder=False)) - before:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.MaterialInterface) or isinstance(a, unreal.Texture):
            log("removing importer extra: %s" % p)
            EAL.delete_asset(p.split(".")[0])
    if mesh is None:
        log("ERROR: no static mesh came out of %s" % fbx)
        return

    mi = AT.create_asset("MI_Car_" + name, DEST, unreal.MaterialInstanceConstant,
                         unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, mat)
    if tex:
        MEL.set_material_instance_texture_parameter_value(mi, "BaseTex", tex)
    if ntex:
        MEL.set_material_instance_texture_parameter_value(mi, "NormalTex", ntex)
    n = mesh.get_num_sections(0) if hasattr(mesh, "get_num_sections") else 1
    mats = mesh.get_editor_property("static_materials")
    for sm in mats:
        sm.set_editor_property("material_interface", mi)
    mesh.set_editor_property("static_materials", mats)
    log("%s material slots: %d (sections %d) -> %s" % (name, len(mats), n, mi.get_name()))

    b = mesh.get_bounding_box()
    log("%s bbox min=%s max=%s tris=%s" % (name, b.min, b.max,
                                            mesh.get_num_triangles(0) if hasattr(mesh, "get_num_triangles") else "?"))


def main():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    art = os.path.join(proj, "Art", "Cars")
    if EAL.does_directory_exist(DEST):
        EAL.delete_directory(DEST)
    mat = make_material()
    for car in CARS:
        import_car(art, *car, mat=mat)
    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        a = unreal.load_asset(p)
        if a:
            EAL.save_loaded_asset(a, False)
    log("assets: %s" % EAL.list_assets(DEST, recursive=True, include_folder=False))


main()
