# Director's Cut — stylised look. Run headless (materials need a real RHI, so no -nullrhi):
#   UnrealEditor-Cmd DirectorsCut.uproject -run=pythonscript -script="Tools/style_materials.py" -unattended -AllowCommandletRendering
# Idempotent. Creates (in /Game/DC/Materials):
#   M_DC_Toon : post-process material (Scene Color Before DOF, so translucency/DOF/TSR all run after it).
#               One pass, 12 scene-texture taps:
#                 - ink outlines on the far side of depth steps and on hard creases (normal change)
#                 - soft cel banding of the *lighting* (scene colour / base colour, posterised in stops), so the
#                   procedural surface patterns are untouched
#               Params: Ink (vec, multiplies the scene under a line), Outline (0..1), Thickness (px at 1080p),
#                       DepthEdge, SoftScale, NormalEdge, Band (0..1), BandStep (stops), BandSoft (0..0.5), FadeStart, FadeEnd (cm)
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/Game/DC/Materials"
MP = unreal.MaterialProperty

TOON_CODE = r"""
const uint IdCol = 14; // PPI_PostProcessInput0
const uint IdDepth = 1; // PPI_SceneDepth
const uint IdNrm = 8;   // PPI_WorldNormal
const uint IdBase = 5;  // PPI_BaseColor
const uint IdSpec = 6;  // PPI_Specular
float2 uv = GetDefaultSceneTextureUV(Parameters, IdCol);
float2 inv = GetSceneTextureBufferSize(IdCol).zw;
float px = max(1.0, Thickness * GetSceneTextureViewSize(IdCol).y / 1080.0);
float2 o = inv * px;

float3 sc = SceneTextureLookup(Parameters, uv, IdCol, false).rgb;
float dc = SceneTextureLookup(Parameters, uv, IdDepth, false).r;
float3 nc = SceneTextureLookup(Parameters, uv, IdNrm, false).xyz;

float2 u1 = ClampSceneTextureUV(uv + o * float2( 1.0,  1.0), IdCol);
float2 u2 = ClampSceneTextureUV(uv + o * float2(-1.0, -1.0), IdCol);
float2 u3 = ClampSceneTextureUV(uv + o * float2( 1.0, -1.0), IdCol);
float2 u4 = ClampSceneTextureUV(uv + o * float2(-1.0,  1.0), IdCol);
float d1 = SceneTextureLookup(Parameters, u1, IdDepth, false).r;
float d2 = SceneTextureLookup(Parameters, u2, IdDepth, false).r;
float d3 = SceneTextureLookup(Parameters, u3, IdDepth, false).r;
float d4 = SceneTextureLookup(Parameters, u4, IdDepth, false).r;
float3 n1 = SceneTextureLookup(Parameters, u1, IdNrm, false).xyz;
float3 n2 = SceneTextureLookup(Parameters, u2, IdNrm, false).xyz;
float3 n3 = SceneTextureLookup(Parameters, u3, IdNrm, false).xyz;
float3 n4 = SceneTextureLookup(Parameters, u4, IdNrm, false).xyz;

// "soft" surfaces (ground + foliage write Specular 0): no crease lines, and only big depth steps draw, so grass
// tufts and leaf facets stay clean while trees, walls, cars and crew still get a silhouette against the ground
float soft = step(SceneTextureLookup(Parameters, uv, IdSpec, false).r, 0.02);
float dT = DepthEdge * lerp(1.0, SoftScale, soft);
// depth: second derivative (zero on flat ground at any angle), only on the far side of a step so lines stay thin
float lap = max(0.0, 2.0 * dc - d1 - d2) + max(0.0, 2.0 * dc - d3 - d4);
float eD = smoothstep(dT, dT * 2.5, lap / max(dc, 1.0));
// creases: only hard corners (soft bevels stay clean)
float dn = 1.0 - min(min(dot(nc, n1), dot(nc, n2)), min(dot(nc, n3), dot(nc, n4)));
float eN = smoothstep(NormalEdge, NormalEdge * 1.8, dn) * (1.0 - soft);
float fade = 1.0 - 0.5 * saturate((dc - FadeStart) / max(FadeEnd - FadeStart, 1.0));
float edge = saturate(max(eD, eN * 0.7)) * Outline * fade;

// cel banding of the lighting term only
float3 lw = float3(0.2126, 0.7152, 0.0722);
float3 bc = SceneTextureLookup(Parameters, uv, IdBase, false).rgb;
float L = dot(sc, lw) / max(dot(bc, lw), 0.02);
float s = log2(max(L, 1e-5)) / BandStep;
float f = frac(s);
float k = exp2((smoothstep(0.5 - BandSoft, 0.5 + BandSoft, f) - f) * BandStep);
sc *= lerp(1.0, k, Band * fade);

return lerp(sc, sc * Ink.rgb, edge);
"""


def fresh(name):
    path = f"{MAT_DIR}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


def expr(m, cls, x=-600, y=0):
    return MEL.create_material_expression(m, cls, x, y)


def sparam(m, name, default, y):
    n = expr(m, unreal.MaterialExpressionScalarParameter, -900, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n


def scene_tex(m, tex_id, y):
    n = expr(m, unreal.MaterialExpressionSceneTexture, -1300, y)
    n.set_editor_property("scene_texture_id", tex_id)
    return n


def toon():
    m = fresh("M_DC_Toon")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    try:
        m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)
    except Exception:
        m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_BEFORE_TRANSLUCENCY)
    ink = expr(m, unreal.MaterialExpressionVectorParameter, -900, -200)
    ink.set_editor_property("parameter_name", "Ink")
    ink.set_editor_property("default_value", unreal.LinearColor(0.10, 0.08, 0.15, 1))
    params = [("Ink", ink)]
    for i, (nm, dv) in enumerate([("Outline", 1.0), ("Thickness", 1.3), ("DepthEdge", 0.0016), ("SoftScale", 8.0), ("NormalEdge", 0.35),
                                  ("Band", 0.5), ("BandStep", 1.0), ("BandSoft", 0.3), ("FadeStart", 7000.0), ("FadeEnd", 26000.0)]):
        params.append((nm, sparam(m, nm, dv, -100 + i * 80)))
    # the scene-texture nodes only declare usage (the code samples by id); wire them in so they are not culled
    S = unreal.SceneTextureId
    decl = [("TCol", scene_tex(m, S.PPI_POST_PROCESS_INPUT0, -400)), ("TDepth", scene_tex(m, S.PPI_SCENE_DEPTH, -200)),
            ("TNrm", scene_tex(m, S.PPI_WORLD_NORMAL, 0)), ("TBase", scene_tex(m, S.PPI_BASE_COLOR, 200)),
            ("TSpec", scene_tex(m, S.PPI_SPECULAR, 400))]
    c = expr(m, unreal.MaterialExpressionCustom, -300, 0)
    c.set_editor_property("code", TOON_CODE)
    c.set_editor_property("description", "DCToon")
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for nm, _ in params + decl:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    for nm, src in params:
        if not MEL.connect_material_expressions(src, "", c, nm):
            unreal.log_warning(f"[DC] toon: failed to connect {nm}")
    for nm, src in decl:
        if not MEL.connect_material_expressions(src, "Color", c, nm):
            unreal.log_warning(f"[DC] toon: failed to connect {nm}")
    MEL.connect_material_property(c, "", MP.MP_EMISSIVE_COLOR)
    return m


for f in (toon,):
    try:
        mat = f()
        MEL.recompile_material(mat)
        EAL.save_loaded_asset(mat)
        unreal.log(f"[DC] saved {mat.get_path_name()}")
    except Exception as e:
        unreal.log_error(f"[DC] material {f.__name__} failed: {e}")

unreal.log("[DC] style_materials done")
