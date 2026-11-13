# Director's Cut — Wave 0 asset bootstrap. Run headless:
#   UnrealEditor-Cmd DirectorsCut.uproject -run=pythonscript -script="Tools/create_materials.py" -unattended -nullrhi
# Idempotent: re-creates the master materials and makes sure L_Hideout exists.
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/Game/DC/Materials"


def fresh_material(name):
    path = f"{MAT_DIR}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    # the scene batches props into instanced static meshes: every project material must allow it
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


def vparam(m, name, default, x, y):
    n = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*default))
    return n


def sparam(m, name, default, x, y):
    n = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n


def mul(m, a, b, x, y):
    n = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, "", n, "A")
    MEL.connect_material_expressions(b, "", n, "B")
    return n


MP = unreal.MaterialProperty


def surface():
    m = fresh_material("M_DC_Surface")
    bc = vparam(m, "BaseColor", (0.2, 0.2, 0.2, 1), -600, -200)
    r = sparam(m, "Roughness", 0.6, -600, 0)
    mt = sparam(m, "Metallic", 0.0, -600, 100)
    sp = sparam(m, "Specular", 0.5, -600, 200)
    ec = vparam(m, "EmissiveColor", (0, 0, 0, 1), -800, 300)
    es = sparam(m, "EmissiveStrength", 0.0, -800, 450)
    MEL.connect_material_property(bc, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(mt, "", MP.MP_METALLIC)
    MEL.connect_material_property(sp, "", MP.MP_SPECULAR)
    MEL.connect_material_property(mul(m, ec, es, -400, 350), "", MP.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    return m


def emissive():
    m = fresh_material("M_DC_Emissive")
    c = vparam(m, "Color", (1, 0.3, 0.7, 1), -600, 0)
    s = sparam(m, "Strength", 10.0, -600, 200)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    MEL.connect_material_property(mul(m, c, s, -300, 100), "", MP.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    return m


def criminal():
    m = fresh_material("M_DC_Criminal")
    bc = vparam(m, "BaseColor", (0.02, 0.02, 0.025, 1), -800, -300)
    ac = vparam(m, "AccentColor", (1, 0.4, 0.1, 1), -900, 200)
    rs = sparam(m, "RimStrength", 4.0, -900, 400)
    r = sparam(m, "Roughness", 0.8, -800, -100)
    fr = MEL.create_material_expression(m, unreal.MaterialExpressionFresnel, -900, 0)
    fr.set_editor_property("exponent", 3.0)
    rim = mul(m, mul(m, fr, ac, -600, 100), rs, -400, 200)
    MEL.connect_material_property(bc, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(rim, "", MP.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(m)
    return m


def glass():
    m = fresh_material("M_DC_Glass")
    c = vparam(m, "Color", (0.6, 0.7, 0.8, 1), -600, 0)
    o = sparam(m, "Opacity", 0.25, -600, 200)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    MEL.connect_material_property(c, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(o, "", MP.MP_OPACITY)
    MEL.recompile_material(m)
    return m


for f in (surface, emissive, criminal, glass):
    mat = f()
    EAL.save_loaded_asset(mat)
    unreal.log(f"[DC] saved {mat.get_path_name()}")

MAP = "/Game/DC/Maps/L_Hideout"
if not EAL.does_asset_exist(MAP):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level(MAP)
    les.save_current_level()
    unreal.log(f"[DC] created {MAP}")
else:
    unreal.log(f"[DC] {MAP} exists")
