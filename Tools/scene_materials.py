# Director's Cut — Agent D scene materials. Run headless (see Tools/create_materials.py for the pattern):
#   UnrealEditor-Cmd DirectorsCut.uproject -run=pythonscript -script="Tools/scene_materials.py" -unattended
# Idempotent. Creates (in /Game/DC/Materials):
#   M_DC_Highlight : translucent ADDITIVE fresnel glow overlay. Params Color(vec), Strength
#   M_DC_Proc      : opaque lit procedural surface (world-aligned HLSL patterns, no textures).
#                    Params ColorA, ColorB, EmissiveColor (vec); Pattern, Scale, Roughness, Metallic, Dirt, Axis, EmissiveStrength
#                    Pattern: 0 plain/grime 1 wood 2 brick 3 concrete 4 cork 5 fabric 6 leather 7 metal 8 tiles
#                             9 hazard stripes 10 corrugated (Axis 0 horizontal ribs, 1 vertical) 11 painted wall
#                             12 cloth 13 rubber mat 14 polished concrete slab (saw cuts) 15 painted block wall
#                             16 diamond plate 17 plywood/OSB 18 asphalt
#   M_DC_Neon      : unlit emissive with flicker. Color, Strength, Flicker(0..1), Seed
#   M_DC_Beam      : translucent additive soft-edged beam (fake light shafts). Color, Strength, Flicker
#   M_DC_Rain      : translucent window with running rain streaks. Color, Opacity, Strength
#   M_DC_Screen    : unlit monitor content (CCTV feed / data UI / city map). Color, Strength, Mode(0..2), Seed
#   M_DC_Window    : unlit factory-window glass that glows with the sky. SkyColor, Strength, Grime
#   M_DC_Fence     : masked two-sided chain-link mesh (casts shadow patterns). Color, Scale, Roughness
#   M_DC_Smoke     : translucent drifting smoke wisps. Color, Opacity
#   M_DC_Dust      : additive dust motes, WPO drift per instance (ISM). Color, Strength, Drift
#   M_DC_GlassI    : translucent lit glass usable on instanced meshes. Color, Opacity, Roughness
#   M_DC_Ground    : opaque lit procedural terrain (world-aligned). ColorA/B/C, FarColor (vec); Pattern, Scale, Roughness,
#                    Axis, Center, Width, FarStart, FarEnd. Pattern: 0 meadow 1 mown lawn 2 gravel 3 dirt track (ruts,
#                    grass centre strip; Axis 0 = along X, Center/Width across) 4 ploughed rows 5 mud/soil
#                    Paint 1 = blend the site's bare ground into this surface with soft ragged edges: SiteYard (min xy,
#                    max xy), SiteTrack (lane Y, lane half-width, drive X, drive half-width), Seg0..6 (footpath a.xy, b.xy),
#                    GravA/GravB, DirtA/DirtB (vec)
#   M_DC_Foliage   : two-sided lit foliage, per-instance hue (ColorA..ColorB), UpN (1 = lit as flat ground), macro tint ColorC (CMix), vertex colour
#                    R = height/AO, G = wind weight. Wind WPO. Params ColorA, ColorB, ColorC, CMix, Wind, Roughness, AOMin
#   M_DC_Water     : opaque glossy water with animated ripples + night glow. Color, Deep, GlowColor, Glow, Ripple
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/Game/DC/Materials"
MP = unreal.MaterialProperty


def fresh_material(name):
    path = f"{MAT_DIR}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    # the scene batches decoration into instanced static meshes: every scene material must allow it
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


def vparam(m, name, default, x=-900, y=0):
    n = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*default))
    return n


def sparam(m, name, default, x=-900, y=0):
    n = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n


def expr(m, cls, x=-600, y=0):
    return MEL.create_material_expression(m, cls, x, y)


def mask(m, src, r, g, b, a, x=-200, y=0):
    n = expr(m, unreal.MaterialExpressionComponentMask, x, y)
    n.set_editor_property("r", r)
    n.set_editor_property("g", g)
    n.set_editor_property("b", b)
    n.set_editor_property("a", a)
    MEL.connect_material_expressions(src, "", n, "")
    return n


def mul(m, a, b, x=-300, y=0):
    n = expr(m, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, "", n, "A")
    MEL.connect_material_expressions(b, "", n, "B")
    return n


def custom(m, code, inputs, out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, x=-500, y=0, desc="DC"):
    """inputs: list of (name, expression)."""
    c = expr(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("description", desc)
    c.set_editor_property("output_type", out_type)
    ins = []
    for nm, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    for nm, src in inputs:
        ok = MEL.connect_material_expressions(src, "", c, nm)
        if not ok:
            unreal.log_warning(f"[DC] failed to connect custom input {nm}")
    return c


NOISE_LIB = r"""
struct DCN {
  float h(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
  float n(float3 x) {
    float3 i = floor(x); float3 f = frac(x); f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(h(i + float3(0,0,0)), h(i + float3(1,0,0)), f.x), lerp(h(i + float3(0,1,0)), h(i + float3(1,1,0)), f.x), f.y),
                lerp(lerp(h(i + float3(0,0,1)), h(i + float3(1,0,1)), f.x), lerp(h(i + float3(0,1,1)), h(i + float3(1,1,1)), f.x), f.y), f.z);
  }
  float fbm(float3 p) { float a = 0.5; float s = 0.0; for (int k = 0; k < 4; k++) { s += a * n(p); p = p * 2.03 + 17.1; a *= 0.5; } return s; }
};
DCN d;
"""

PROC_CODE = NOISE_LIB + r"""
float3 Nw = normalize(N);
float3 an = abs(Nw);
float2 uv = (an.z > 0.7) ? P.xy : ((an.x > an.y) ? float2(P.y, P.z) : float2(P.x, P.z));
float sc = max(Scale, 0.001);
float3 q = P * sc;
float3 A = ColorA.rgb; float3 B = ColorB.rgb;
float3 col = A; float rough = Rough; float cav = 0.0;
int pat = (int)round(Pattern);
if (pat == 1) { // wood
  float along = lerp(q.x, q.y, Axis); float across = lerp(q.y, q.x, Axis);
  float w = d.fbm(float3(along * 0.004, across * 0.05, q.z * 0.05));
  float rings = frac(across * 0.06 + q.z * 0.06 + w * 3.0);
  float fine = d.n(float3(along * 0.01, across * 1.2, q.z * 1.2));
  float g = pow(rings, 2.5);
  col = lerp(A, B, saturate(g * 0.75 + fine * 0.35));
  rough = Rough + fine * 0.12; cav = g * 0.15;
} else if (pat == 2) { // brick
  float2 b = uv * sc / float2(48.0, 17.0);
  float row = floor(b.y); b.x += frac(row * 0.5);
  float2 id = floor(b); float2 f = frac(b);
  float2 e = float2(0.035, 0.1);
  float2 mm = smoothstep(0.0, e, f) * smoothstep(0.0, e, 1.0 - f);
  float br = mm.x * mm.y;
  float r = d.h(float3(id, 3.7));
  float sp = d.n(P * 0.35);
  float3 bc = A * (0.7 + 0.55 * r) * (0.85 + 0.3 * sp);
  col = lerp(B, bc, br);
  rough = lerp(0.98, Rough, br); cav = (1.0 - br) * 0.5;
} else if (pat == 3) { // concrete
  float g = d.fbm(q * 0.008);
  float sp = d.n(q * 0.6);
  col = lerp(B, A, saturate(g * 1.3 - 0.1)) * (0.9 + 0.2 * sp);
  rough = Rough + sp * 0.08;
} else if (pat == 4) { // cork
  float s = d.n(q * 0.5) * 0.55 + d.n(q * 1.6) * 0.45;
  col = lerp(A, B, smoothstep(0.35, 0.75, s));
  rough = 0.92; cav = (1.0 - s) * 0.2;
} else if (pat == 5) { // upholstery fabric
  float g = d.fbm(q * 0.05);
  float wv = 0.9 + 0.1 * sin(uv.x * sc * 2.5) * sin(uv.y * sc * 2.5);
  col = lerp(A, B, saturate(g * 1.2 - 0.3)) * wv;
  rough = 0.95;
} else if (pat == 6) { // worn leather
  float c = abs(d.n(q * 0.12) * 2.0 - 1.0);
  float cr = smoothstep(0.0, 0.1, c);
  float wear = smoothstep(0.5, 0.75, d.fbm(q * 0.02));
  col = lerp(A, B, wear) * lerp(0.55, 1.0, cr);
  rough = lerp(Rough, Rough + 0.25, wear);
} else if (pat == 7) { // metal / brass (brushed + tarnish)
  float s = d.n(float3(q.x * 0.01, q.y * 3.0, q.z * 3.0)) * 0.5 + d.n(float3(q.x * 3.0, q.y * 0.01, q.z * 3.0)) * 0.5;
  float t = smoothstep(0.5, 0.8, d.fbm(q * 0.03));
  col = lerp(A, B, t) * (0.85 + 0.25 * s);
  rough = Rough + t * 0.35 + s * 0.06;
} else if (pat == 8) { // checker tiles
  float2 t = P.xy * sc / 60.0;
  float2 id = floor(t); float2 f = frac(t);
  float chk = frac((id.x + id.y) * 0.5) * 2.0;
  float2 mm = smoothstep(0.0, 0.03, f) * smoothstep(0.0, 0.03, 1.0 - f);
  float gr = mm.x * mm.y;
  float r = d.h(float3(id, 1.3));
  float wear = d.fbm(q * 0.006);
  col = lerp(A, B, chk) * (0.85 + 0.25 * r) * lerp(1.0, 0.7, smoothstep(0.45, 0.8, wear));
  col = lerp(col * 0.35, col, gr);
  rough = lerp(0.95, Rough + wear * 0.3, gr); cav = (1.0 - gr) * 0.3;
} else if (pat == 9) { // hazard stripes
  float s = frac((uv.x + uv.y) * sc / 16.0);
  col = (s < 0.5) ? A : B;
  col *= 0.85 + 0.25 * d.n(q * 0.2);
} else if (pat == 10) { // corrugated metal
  float c = sin(lerp(uv.y, uv.x, Axis) * sc * 0.7) * 0.5 + 0.5;
  float rust = smoothstep(0.45, 0.8, d.fbm(q * 0.01 + float3(0, 0, q.z * 0.01)));
  col = lerp(A, B, rust) * (0.6 + 0.45 * c);
  rough = Rough + rust * 0.4;
} else if (pat == 11) { // painted wall: A above, darker B band below 110cm
  float lo = 1.0 - smoothstep(108.0, 110.0, P.z);
  float g = d.fbm(q * 0.01);
  col = lerp(A, B, lo) * (0.85 + 0.25 * g);
} else if (pat == 12) { // draped cloth / dust sheet
  float folds = d.fbm(float3(q.x * 0.02, q.y * 0.02, q.z * 0.006));
  float fine = d.n(q * 0.8);
  col = A * (0.62 + 0.45 * folds) * (0.95 + 0.08 * fine);
  rough = 0.95;
} else if (pat == 13) { // ribbed rubber mat
  float r = sin(uv.x * sc * 1.6) * 0.5 + 0.5;
  float fz = d.n(q * 1.5);
  col = lerp(B, A, r) * (0.85 + 0.25 * fz);
  rough = Rough + (1.0 - r) * 0.1;
} else if (pat == 14) { // polished concrete slab: mottled trowel, saw-cut joints every 250cm, worn polish
  float g = d.fbm(q * 0.006);
  float m2 = d.fbm(q * 0.04 + 5.0);
  float sp = d.n(q * 0.9);
  float2 j = abs(frac(P.xy / 250.0 + 0.5) - 0.5) * 250.0;
  float joint = 1.0 - smoothstep(0.4, 1.4, min(j.x, j.y));
  col = lerp(B, A, saturate(g * 1.5 - 0.2)) * (0.88 + 0.18 * m2) * (0.95 + 0.1 * sp);
  float stain = smoothstep(0.6, 0.85, d.fbm(q * 0.003 + 9.0));
  col *= lerp(1.0, 0.7, stain);
  col = lerp(col, col * 0.25, joint);
  rough = saturate(Rough + (m2 - 0.5) * 0.35 + stain * 0.2 + joint * 0.5);
} else if (pat == 15) { // painted concrete block (CMU) 40x20, scuffed lower band
  float2 b = uv * sc / float2(40.0, 20.0);
  float row = floor(b.y); b.x += frac(row * 0.5);
  float2 f = frac(b);
  float2 mm = smoothstep(0.0, 0.025, f) * smoothstep(0.0, 0.025, 1.0 - f);
  float br = mm.x * mm.y;
  float g = d.fbm(q * 0.015);
  float scuff = (1.0 - smoothstep(20.0, 90.0, P.z)) * d.fbm(q * 0.05);
  col = lerp(A * 0.82, A * (0.92 + 0.15 * g), br);
  col = lerp(col, B, saturate(scuff * 1.4));
  rough = Rough; cav = (1.0 - br) * 0.15;
} else if (pat == 16) { // diamond plate
  float2 u = uv * sc / 3.2;
  float2 c = frac(u) - 0.5;
  float odd = fmod(floor(u.x) + floor(u.y), 2.0);
  float2 r = odd > 0.5 ? float2(c.x + c.y, c.x - c.y) : float2(c.x - c.y, c.x + c.y);
  float bump = 1.0 - smoothstep(0.05, 0.09, abs(r.x) * 0.25 + abs(r.y) * 0.225);
  float t = smoothstep(0.45, 0.8, d.fbm(q * 0.02));
  col = lerp(A, B, t) * (0.75 + 0.45 * bump);
  rough = Rough - bump * 0.15 + t * 0.3;
} else if (pat == 17) { // plywood / OSB chips
  float chips = d.n(q * 0.35);
  float ch2 = d.n(float3(q.x * 0.9 + 3.0, q.y * 0.15, q.z * 0.9));
  col = lerp(A, B, smoothstep(0.3, 0.8, chips * 0.6 + ch2 * 0.4));
  rough = 0.85;
} else if (pat == 18) { // asphalt: aggregate, tar patches, cracks
  float ag = d.n(q * 2.5);
  float tar = smoothstep(0.55, 0.7, d.fbm(q * 0.004));
  float cr = 1.0 - smoothstep(0.0, 0.02, abs(d.fbm(q * 0.01) - 0.5));
  col = A * (0.75 + 0.5 * ag);
  col = lerp(col, B, tar);
  col *= (1.0 - cr * 0.6);
  rough = Rough - tar * 0.25;
} else { // plain + grime
  float g = d.fbm(q * 0.02);
  col = lerp(A, B, saturate(g * 1.4 - 0.35));
}
float grime = d.fbm(P * 0.0035 + 3.1);
col *= lerp(1.0, 0.6, saturate(grime * Dirt * 1.5)) * (1.0 - cav);
return float4(saturate(col), saturate(rough));
"""


def proc():
    m = fresh_material("M_DC_Proc")
    ca = vparam(m, "ColorA", (0.3, 0.2, 0.12, 1), -1200, -400)
    cb = vparam(m, "ColorB", (0.1, 0.06, 0.04, 1), -1200, -200)
    pat = sparam(m, "Pattern", 0.0, -1200, 0)
    scl = sparam(m, "Scale", 1.0, -1200, 100)
    rgh = sparam(m, "Roughness", 0.6, -1200, 200)
    dirt = sparam(m, "Dirt", 0.5, -1200, 300)
    axis = sparam(m, "Axis", 0.0, -1200, 400)
    met = sparam(m, "Metallic", 0.0, -600, 300)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1200, 500)
    nrm = expr(m, unreal.MaterialExpressionVertexNormalWS, -1200, 600)
    cam = expr(m, unreal.MaterialExpressionCameraVectorWS, -1200, 700)
    c = custom(m, PROC_CODE, [("P", wp), ("N", nrm), ("V", cam), ("ColorA", ca), ("ColorB", cb), ("Pattern", pat),
                              ("Scale", scl), ("Rough", rgh), ("Dirt", dirt), ("Axis", axis)],
               out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4, desc="DCProc")
    MEL.connect_material_property(mask(m, c, True, True, True, False, -200, -200), "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(mask(m, c, False, False, False, True, -200, 0), "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(met, "", MP.MP_METALLIC)
    ec = vparam(m, "EmissiveColor", (0, 0, 0, 1), -800, 500)
    es = sparam(m, "EmissiveStrength", 0.0, -800, 650)
    MEL.connect_material_property(mul(m, ec, es, -400, 550), "", MP.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


def highlight():
    m = fresh_material("M_DC_Highlight")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", False)
    c = vparam(m, "Color", (1.0, 0.75, 0.35, 1), -900, -100)
    s = sparam(m, "Strength", 2.0, -900, 100)
    nrm = expr(m, unreal.MaterialExpressionVertexNormalWS, -900, 250)
    cam = expr(m, unreal.MaterialExpressionCameraVectorWS, -900, 350)
    t = expr(m, unreal.MaterialExpressionTime, -900, 450)
    code = r"""
float fr = pow(1.0 - saturate(abs(dot(normalize(N), normalize(V)))), 3.0);
// x3 gain: the scene runs at a dark manual exposure (EV bias ~-2.5), callers pass Strength ~0.7..1.4
return Color.rgb * Strength * 3.0 * (0.04 + fr);
"""
    cu = custom(m, code, [("N", nrm), ("V", cam), ("T", t), ("Color", c), ("Strength", s)])
    MEL.connect_material_property(cu, "", MP.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


def neon():
    m = fresh_material("M_DC_Neon")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    c = vparam(m, "Color", (1.0, 0.2, 0.6, 1), -900, -100)
    s = sparam(m, "Strength", 20.0, -900, 50)
    fl = sparam(m, "Flicker", 0.3, -900, 150)
    sd = sparam(m, "Seed", 0.0, -900, 250)
    t = expr(m, unreal.MaterialExpressionTime, -900, 350)
    code = r"""
float tt = T + Seed * 13.7;
float k = floor(tt * 14.0);
float hsh = frac(sin(k * 12.9898 + Seed * 78.233) * 43758.5453);
float burst = step(0.82, frac(sin(floor(tt * 0.7) * 3.17 + Seed) * 9631.1));
float on = (hsh < Flicker * burst) ? 0.08 : 1.0;
float hum = 0.94 + 0.06 * sin(tt * 120.0);
return Color.rgb * Strength * on * hum;
"""
    cu = custom(m, code, [("T", t), ("Color", c), ("Strength", s), ("Flicker", fl), ("Seed", sd)])
    MEL.connect_material_property(cu, "", MP.MP_EMISSIVE_COLOR)
    return m


def beam():
    m = fresh_material("M_DC_Beam")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    c = vparam(m, "Color", (0.9, 0.85, 0.7, 1), -900, -100)
    s = sparam(m, "Strength", 0.4, -900, 50)
    fl = sparam(m, "Flicker", 0.25, -900, 150)
    nrm = expr(m, unreal.MaterialExpressionVertexNormalWS, -900, 250)
    cam = expr(m, unreal.MaterialExpressionCameraVectorWS, -900, 350)
    t = expr(m, unreal.MaterialExpressionTime, -900, 450)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -900, 550)
    df = expr(m, unreal.MaterialExpressionDepthFade, -600, 600)
    df.set_editor_property("fade_distance_default", 40.0)
    lp = expr(m, unreal.MaterialExpressionTransformPosition, -700, 700)
    lp.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    lp.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    MEL.connect_material_expressions(wp, "", lp, "")
    code = NOISE_LIB + r"""
// beams are unit cylinders (local z -50..50, light travels +z): fade in from the opening, out towards the end; kills the caps
float lz = LP.z / 50.0;
float along = smoothstep(-1.0, -0.75, lz) * (1.0 - smoothstep(0.1, 0.95, lz));
float soft = pow(saturate(abs(dot(normalize(N), normalize(V)))), 2.0) * along;
float k = floor(T * 22.0);
float f = 1.0 - Flicker * frac(sin(k * 91.7) * 43758.5);
float motes = 0.75 + 0.5 * d.n(P * 0.05 + float3(0, 0, T * 8.0));
return Color.rgb * Strength * soft * f * motes * DF;
"""
    cu = custom(m, code, [("N", nrm), ("V", cam), ("T", t), ("P", wp), ("Color", c), ("Strength", s), ("Flicker", fl), ("DF", df), ("LP", lp)])
    MEL.connect_material_property(cu, "", MP.MP_EMISSIVE_COLOR)
    return m


def rain():
    m = fresh_material("M_DC_Rain")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    c = vparam(m, "Color", (0.35, 0.55, 0.75, 1), -900, -100)
    o = sparam(m, "Opacity", 0.18, -900, 50)
    s = sparam(m, "Strength", 1.5, -900, 150)
    t = expr(m, unreal.MaterialExpressionTime, -900, 250)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -900, 350)
    code = NOISE_LIB + r"""
float u = (P.x + P.y) * 0.25; float v = P.z * 0.02;
float col = floor(u);
float r = d.h(float3(col, 1.7, 0.3));
float sp = 0.4 + r * 0.9;
float y = frac(v * (0.6 + r) + T * sp + r * 7.0);
float streak = smoothstep(0.0, 0.08, y) * smoothstep(0.35, 0.08, y);
float w = smoothstep(0.5, 0.15, abs(frac(u) - 0.5));
float drops = step(0.93, d.n(float3(u * 3.0, v * 30.0, floor(T * 0.5)))) * 0.6;
float a = saturate(streak * w * step(0.35, r) + drops);
return float4(Color.rgb * (0.25 + a * Strength), saturate(Opacity + a * 0.45));
"""
    cu = custom(m, code, [("P", wp), ("T", t), ("Color", c), ("Opacity", o), ("Strength", s)],
                out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    MEL.connect_material_property(mask(m, cu, True, True, True, False), "", MP.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(mask(m, cu, False, False, False, True, -200, 200), "", MP.MP_OPACITY)
    return m


def screen():
    m = fresh_material("M_DC_Screen")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    c = vparam(m, "Color", (0.55, 0.8, 1.0, 1), -900, -100)
    s = sparam(m, "Strength", 3.0, -900, 50)
    md = sparam(m, "Mode", 0.0, -900, 120)
    sd = sparam(m, "Seed", 0.0, -900, 190)
    t = expr(m, unreal.MaterialExpressionTime, -900, 260)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 330)
    pr = expr(m, unreal.MaterialExpressionPerInstanceRandom, -900, 400)
    op = expr(m, unreal.MaterialExpressionObjectPositionWS, -900, 470)
    code = NOISE_LIB + r"""
float id = frac(R * 7.31 + d.h(floor(OP * 0.37) + Seed));
float2 u = UV;
float scan = 0.88 + 0.12 * sin(u.y * 900.0 + T * 6.0);
float2 c2 = u - 0.5;
float vig = saturate(1.1 - dot(c2, c2) * 1.6);
int mode = (int)round(Mode);
float3 col;
if (mode == 0) {
  // CCTV feed: grey-green low-contrast scene that cuts every few seconds, timestamp bar, REC dot, noise
  float cut = floor(T * 0.12 + id * 9.0);
  float3 sp = float3(u * float2(3.0, 2.0), cut * 3.7 + id * 11.0);
  float scene = d.fbm(sp) * 0.7 + 0.3 * step(0.55, d.n(float3(floor(u.x * 6.0 + id * 3.0), floor(u.y * 4.0), cut)));
  float walker = smoothstep(0.035, 0.0, length((u - float2(frac(T * 0.05 + id), 0.62)) * float2(1.0, 0.45)));
  float noise = d.h(float3(u * 300.0, floor(T * 20.0))) * 0.18;
  float bar = step(0.9, u.y) * step(u.x, 0.45) * step(0.5, frac(u.x * 40.0));
  float rec = step(length(u - float2(0.92, 0.08)), 0.025) * step(0.5, frac(T * 0.8));
  float lum = scene * 0.75 + walker * 0.6 + noise + bar * 0.5;
  col = float3(0.75, 0.9, 0.8) * lum + float3(1.0, 0.1, 0.05) * rec * 2.0;
} else if (mode == 1) {
  // data UI: dark panel, scrolling rows, bar graph, waveform
  float rows = step(0.55, d.h(float3(floor(u.y * 28.0 - T * 1.5), floor(u.x * 9.0), id))) * step(0.5, frac(u.y * 28.0 - T * 1.5)) * step(u.x, 0.55);
  float bx = floor((u.x - 0.6) * 12.0);
  float bh = d.n(float3(bx, T * 0.4, id * 5.0));
  float bars = step(0.6, u.x) * step(0.5, frac((u.x - 0.6) * 12.0)) * step(1.0 - u.y, bh * 0.6);
  float wave = smoothstep(0.012, 0.0, abs(u.y - 0.2 - 0.07 * sin(u.x * 30.0 + T * 3.0 + id * 6.0))) * step(0.6, u.x);
  float frame = step(0.97, max(abs(c2.x), abs(c2.y)) * 2.0);
  col = float3(0.03, 0.06, 0.1) + Color.rgb * (rows * 0.5 + bars * 0.7 + wave + frame * 0.3);
} else {
  // city map: dark street grid, avenues, river, pulsing target pins
  float2 g = u * float2(16.0, 10.0);
  float2 gf = abs(frac(g) - 0.5);
  float streets = smoothstep(0.42, 0.47, max(gf.x, gf.y));
  float avenue = smoothstep(0.02, 0.0, abs(u.x - u.y * 0.6 - 0.2)) + smoothstep(0.015, 0.0, abs(u.y - 0.55 - 0.05 * sin(u.x * 8.0)));
  float river = smoothstep(0.05, 0.03, abs(u.y - 0.3 - 0.08 * sin(u.x * 5.0 + 1.0)));
  float blocks = d.h(float3(floor(g), 4.0));
  col = float3(0.02, 0.03, 0.04) + float3(0.05, 0.07, 0.09) * blocks * (1.0 - streets) + float3(0.25, 0.3, 0.35) * streets * 0.35
      + float3(0.6, 0.65, 0.7) * saturate(avenue) * 0.35 + float3(0.02, 0.08, 0.15) * river;
  for (int k = 0; k < 6; k++) {
    float2 p = float2(d.h(float3(k, 1.0, id)), d.h(float3(k, 2.0, id))) * 0.8 + 0.1;
    float r = length((u - p) * float2(1.6, 1.0));
    float pulse = frac(T * 0.5 + k * 0.17);
    col += Color.rgb * (smoothstep(0.012, 0.004, r) * 2.0 + smoothstep(0.004, 0.0, abs(r - pulse * 0.06)) * (1.0 - pulse));
  }
}
return col * Strength * scan * vig;
"""
    cu = custom(m, code, [("T", t), ("UV", uv), ("R", pr), ("OP", op), ("Color", c), ("Strength", s), ("Mode", md), ("Seed", sd)])
    MEL.connect_material_property(cu, "", MP.MP_EMISSIVE_COLOR)
    return m


def window():
    m = fresh_material("M_DC_Window")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    c = vparam(m, "SkyColor", (0.6, 0.65, 0.7, 1), -900, -100)
    s = sparam(m, "Strength", 1.0, -900, 50)
    g = sparam(m, "Grime", 0.6, -900, 150)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -900, 250)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 350)
    code = NOISE_LIB + r"""
// frosted / wired factory glass: per-pane brightness, vertical sky gradient, grime toward the pane edges
float2 c2 = abs(UV - 0.5) * 2.0;
float edge = smoothstep(0.55, 1.0, max(c2.x, c2.y));
float pane = d.h(floor(P / 20.0) + 1.3);
float grime = d.fbm(P * 0.05) * Grime;
float wire = step(0.92, frac((P.x + P.y) * 0.4)) + step(0.92, frac(P.z * 0.4));
float grad = 0.75 + 0.35 * saturate((P.z - 150.0) / 200.0);
float k = (0.65 + 0.35 * pane) * grad * (1.0 - 0.55 * edge) * (1.0 - 0.6 * grime) * (1.0 - 0.15 * saturate(wire));
if (pane > 0.93) k *= 0.25;
return Color.rgb * Strength * k;
"""
    cu = custom(m, code, [("P", wp), ("UV", uv), ("Color", c), ("Strength", s), ("Grime", g)])
    MEL.connect_material_property(cu, "", MP.MP_EMISSIVE_COLOR)
    return m


def fence():
    m = fresh_material("M_DC_Fence")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    c = vparam(m, "Color", (0.35, 0.36, 0.37, 1), -900, -100)
    sc = sparam(m, "Scale", 1.0, -900, 50)
    r = sparam(m, "Roughness", 0.45, -900, 150)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -900, 250)
    nrm = expr(m, unreal.MaterialExpressionVertexNormalWS, -900, 350)
    code = r"""
float3 an = abs(normalize(N));
float2 uv = (an.z > 0.7) ? P.xy : ((an.x > an.y) ? float2(P.y, P.z) : float2(P.x, P.z));
uv *= Scale / 6.0;
float2 a = float2(uv.x + uv.y, uv.x - uv.y);
float2 f = abs(frac(a) - 0.5);
return smoothstep(0.43, 0.47, max(f.x, f.y));
"""
    cu = custom(m, code, [("P", wp), ("N", nrm), ("Scale", sc)], out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    MEL.connect_material_property(cu, "", MP.MP_OPACITY_MASK)
    MEL.connect_material_property(c, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    k = expr(m, unreal.MaterialExpressionConstant, -600, 400)
    k.set_editor_property("r", 1.0)
    MEL.connect_material_property(k, "", MP.MP_METALLIC)
    return m


def smoke():
    m = fresh_material("M_DC_Smoke")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    c = vparam(m, "Color", (0.55, 0.55, 0.6, 1), -900, -100)
    o = sparam(m, "Opacity", 0.35, -900, 50)
    nrm = expr(m, unreal.MaterialExpressionVertexNormalWS, -900, 150)
    cam = expr(m, unreal.MaterialExpressionCameraVectorWS, -900, 250)
    t = expr(m, unreal.MaterialExpressionTime, -900, 350)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -900, 450)
    code = NOISE_LIB + r"""
float soft = pow(saturate(abs(dot(normalize(N), normalize(V)))), 3.0);
float n = d.fbm(P * 0.06 + float3(sin(T * 0.3) * 2.0, 0, -T * 1.2));
float a = saturate((n - 0.42) * 2.5) * soft * Opacity;
return float4(Color.rgb, a);
"""
    cu = custom(m, code, [("N", nrm), ("V", cam), ("T", t), ("P", wp), ("Color", c), ("Opacity", o)],
                out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    MEL.connect_material_property(mask(m, cu, True, True, True, False), "", MP.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(mask(m, cu, False, False, False, True, -200, 200), "", MP.MP_OPACITY)
    return m


def dust():
    m = fresh_material("M_DC_Dust")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    c = vparam(m, "Color", (1.0, 0.85, 0.6, 1), -900, -100)
    s = sparam(m, "Strength", 2.0, -900, 50)
    dr = sparam(m, "Drift", 25.0, -900, 150)
    t = expr(m, unreal.MaterialExpressionTime, -900, 250)
    pr = expr(m, unreal.MaterialExpressionPerInstanceRandom, -900, 350)
    em_code = r"""
float tw = 0.55 + 0.45 * sin(T * (1.0 + R * 2.0) + R * 40.0);
return Color.rgb * Strength * tw;
"""
    em = custom(m, em_code, [("T", t), ("R", pr), ("Color", c), ("Strength", s)], y=-200)
    MEL.connect_material_property(em, "", MP.MP_EMISSIVE_COLOR)
    wpo_code = r"""
float r = R * 6.2831;
return float3(sin(T * 0.21 + r * 7.0) + 0.5 * sin(T * 0.53 + r * 3.0),
              cos(T * 0.17 + r * 5.0) + 0.5 * cos(T * 0.47 + r * 11.0),
              sin(T * 0.13 + r * 9.0) * 0.8) * Drift;
"""
    wpo = custom(m, wpo_code, [("T", t), ("R", pr), ("Drift", dr)], y=200)
    MEL.connect_material_property(wpo, "", MP.MP_WORLD_POSITION_OFFSET)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


import os
_only = [x for x in os.environ.get("DC_MATS", "").split(",") if x]
def glassi():
    m = fresh_material("M_DC_GlassI")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    c = vparam(m, "Color", (0.4, 0.5, 0.55, 1), -600, 0)
    o = sparam(m, "Opacity", 0.25, -600, 200)
    r = sparam(m, "Roughness", 0.05, -600, 300)
    MEL.connect_material_property(c, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(o, "", MP.MP_OPACITY)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    return m


GROUND_CODE = NOISE_LIB + r"""
float3 A = ColorA.rgb; float3 B = ColorB.rgb; float3 C = ColorC.rgb;
float sc = max(Scale, 0.001);
float3 q = P * sc;
float macro = d.fbm(float3(P.xy * 0.00035, 1.3));
float mid = d.fbm(float3(P.xy * 0.0035, 7.7));
float fine = d.n(float3(P.xy * 0.09, 2.0));
float along = lerp(P.x, P.y, Axis); float across = lerp(P.y, P.x, Axis) - Center;
float3 col = A; float rough = Rough;
int pat = (int)round(Pattern);
if (pat == 1) { // mown lawn: stripes + clover patches
  float s = step(0.5, frac(across / 260.0));
  col = lerp(A, B, smoothstep(0.3, 0.8, macro) * 0.6);
  col = lerp(col, C, smoothstep(0.55, 0.8, mid) * 0.5);
  col *= (s > 0.5 ? 1.07 : 0.93) * (0.9 + 0.2 * fine);
} else if (pat == 2) { // gravel
  float p1 = d.n(q * 0.55); float p2 = d.n(q * 1.7 + 4.0);
  float peb = smoothstep(0.45, 0.65, p1 * 0.6 + p2 * 0.4);
  col = lerp(B, A, peb) * (0.85 + 0.3 * d.n(q * 3.1));
  float patch = smoothstep(0.5, 0.75, d.fbm(q * 0.006));
  col = lerp(col, C, patch * 0.7);
  float weeds = smoothstep(0.66, 0.8, d.fbm(q * 0.02 + 9.0));
  col = lerp(col, float3(0.10, 0.16, 0.05), weeds * 0.8);
  rough = Rough;
} else if (pat == 3) { // dirt track with ruts and a grass centre strip
  float hw = max(Width * 0.5, 1.0);
  float u = abs(across) / hw; // 0 centre .. 1 edge
  float wob = (d.n(float3(along * 0.01, 3.0, 1.0)) - 0.5) * 0.18;
  float rut = exp(-pow((u - 0.5 + wob) / 0.13, 2.0));
  float centre = 1.0 - smoothstep(0.10, 0.22, u + wob * 0.6 + (fine - 0.5) * 0.08);
  float verge = smoothstep(0.80, 0.98, u + wob + (fine - 0.5) * 0.12);
  float3 dirt = lerp(A, B, saturate(rut * 0.8 + (mid - 0.5) * 0.6)) * (0.85 + 0.3 * d.n(q * 0.8));
  float stones = smoothstep(0.72, 0.8, d.n(q * 0.9));
  dirt = lerp(dirt, A * 1.5, stones * 0.5);
  float3 grass = lerp(C, C * float3(1.2, 1.1, 0.7), macro) * (0.85 + 0.3 * fine);
  col = lerp(dirt, grass, saturate(centre + verge));
  rough = lerp(Rough, 0.9, saturate(centre + verge)) - rut * 0.1;
} else if (pat == 4) { // ploughed field rows
  float r = sin(across * 6.2831 / 90.0) * 0.5 + 0.5;
  col = lerp(B, A, r) * (0.85 + 0.25 * d.n(q * 0.5)) * (0.9 + 0.2 * macro);
  rough = Rough;
} else if (pat == 5) { // mud / soil
  col = lerp(A, B, saturate(mid * 1.4 - 0.2)) * (0.85 + 0.3 * fine);
} else { // meadow
  col = lerp(A, B, smoothstep(0.35, 0.72, macro));
  col = lerp(col, C, smoothstep(0.5, 0.78, mid) * 0.55);
  col *= 0.86 + 0.28 * fine;
}
if (Paint > 0.5) { // site paint: yard, lane, driveway and footpaths blended into the grass (no hard-edged skins)
  float2 pp = P.xy;
  // --- signed distances (cm, negative inside); cheap, no noise yet
  float2 yc = (SiteYard.xy + SiteYard.zw) * 0.5; float2 yh = (SiteYard.zw - SiteYard.xy) * 0.5;
  float rad = 170.0;
  float2 qy = abs(pp - yc) - (yh - rad);
  float dYard = length(max(qy, 0.0)) + min(max(qy.x, qy.y), 0.0) - rad;
  float laneA = pp.y - SiteTrack.x; float laneH = SiteTrack.y;
  float drvA = pp.x - SiteTrack.z; float drvH = SiteTrack.w;
  float dLane = abs(laneA) - laneH;
  float dDrv = max(abs(drvA) - drvH, max(SiteTrack.x - pp.y, pp.y - yc.y));
  float kk = 170.0; // fillet where the driveway meets the lane
  float hh = saturate(0.5 + 0.5 * (dDrv - dLane) / kk);
  float dTrack = lerp(dDrv, dLane, hh) - kk * hh * (1.0 - hh);
  float kq = 150.0; // and flares into the yard
  float hq = saturate(0.5 + 0.5 * (dYard - dTrack) / kq);
  float dWide = lerp(dYard, dTrack, hq) - kq * hq * (1.0 - hq);
  float dPath = 1.0e6;
  float4 sg[7] = { Seg0, Seg1, Seg2, Seg3, Seg4, Seg5, Seg6 };
  float2 pm = pp + (float2(d.n(float3(pp * 0.005, 11.0)), d.n(float3(pp * 0.005, 17.0))) - 0.5) * 130.0; // footpaths meander
  for (int si = 0; si < 7; si++) {
    float2 ab = sg[si].zw - sg[si].xy; float2 ap = pm - sg[si].xy;
    float tt = saturate(dot(ap, ab) / max(dot(ab, ab), 1.0));
    dPath = min(dPath, length(ap - ab * tt));
  }
  dPath -= 52.0;
  float dAll = min(dWide, dPath);
  if (dAll < 260.0) {
    float w1 = d.fbm(float3(pp * 0.0045, 5.0)) - 0.47;
    float clump = d.n(float3(pp * 0.04, 8.0)) - 0.5;
    float grit = fine - 0.5;
    float wobY = w1 * 300.0;                      // yard outline wanders about half a metre
    float wobT = w1 * 170.0;                      // tracks stay straighter
    float rag = clump * 70.0 + grit * 22.0;       // ragged, clumpy last few cm
    // dry, trodden grass fringe round all bare ground
    float fringe = 1.0 - smoothstep(0.0, 130.0, min(dWide + wobT, dPath + wobT * 0.5) + clump * 60.0);
    float3 grassc = col;
    col = lerp(col, col * float3(1.22, 1.10, 0.70), fringe * 0.45);
    // --- track dirt with ruts and a grass centre strip
    float rw = (d.n(float3(pp * 0.01, 3.0)) - 0.5) * 0.16;
    float uL = abs(laneA) / laneH; float uD = abs(drvA) / drvH;
    float inDrv = 1.0 - smoothstep(-60.0, 20.0, dDrv);
    float rut = max(exp(-pow((uL - 0.55 + rw) / 0.15, 2.0)), exp(-pow((uD - 0.55 + rw) / 0.17, 2.0)) * inDrv * smoothstep(-60.0, 120.0, dYard));
    float stripL = (1.0 - smoothstep(0.09, 0.22, uL + rw * 0.6 + grit * 0.08)) * smoothstep(drvH + 40.0, drvH + 330.0, abs(drvA));
    float stripD = (1.0 - smoothstep(0.10, 0.26, uD + rw * 0.6 + grit * 0.10)) * inDrv * smoothstep(40.0, 300.0, dLane) * smoothstep(60.0, 300.0, dYard);
    float strip = saturate(max(stripL, stripD));
    float3 dirt = lerp(DirtA.rgb, DirtB.rgb, saturate(rut * 0.75 + (mid - 0.5) * 0.7)) * (0.9 + 0.2 * d.n(float3(pp * 0.22, 4.0)));
    dirt = lerp(dirt, DirtA.rgb * 1.45, smoothstep(0.74, 0.82, d.n(float3(pp * 0.5, 6.0))) * 0.45); // scattered stones
    dirt = lerp(dirt, grassc * float3(1.12, 1.05, 0.8), strip);
    // --- gravel: soft pebble mottling, bare-earth patches, a few weeds, thinning out towards the rim
    float peb = smoothstep(0.38, 0.66, d.n(float3(pp * 0.16, 1.0)) * 0.6 + d.n(float3(pp * 0.45, 5.0)) * 0.4);
    float3 grav = lerp(GravB.rgb, GravA.rgb, 0.35 + 0.65 * peb);
    grav = lerp(grav, DirtA.rgb * 0.85, smoothstep(0.50, 0.74, d.fbm(float3(pp * 0.006, 2.0))) * 0.55);
    grav = lerp(grav, grassc * 0.9, smoothstep(0.70, 0.80, d.fbm(float3(pp * 0.02, 9.0))) * 0.6);
    float dY2 = dYard + wobY;
    float thin = smoothstep(-190.0, -50.0, dY2 + clump * 80.0);
    grav = lerp(grav, DirtA.rgb * 0.9, thin * 0.6);
    float gravM = 1.0 - smoothstep(-30.0, 30.0, dY2 + 45.0 + rag);
    // --- footpaths: narrow trodden earth, soft edged
    float dP2 = dPath + wobT * 0.45 + clump * 26.0 + grit * 14.0;
    float pathM = (1.0 - smoothstep(-34.0, 22.0, dP2)) * 0.94;
    float3 pathc = lerp(DirtA.rgb * 1.08, DirtB.rgb * 1.25, saturate(mid * 1.3 - 0.25)) * (0.9 + 0.2 * fine);
    // --- compose: grass -> bare earth (ragged) -> gravel on top
    float hq2 = saturate(0.5 + 0.5 * (dY2 - (dTrack + wobT)) / kq);
    float dW2 = lerp(dY2, dTrack + wobT, hq2) - kq * hq2 * (1.0 - hq2);
    float earthM = 1.0 - smoothstep(-24.0, 24.0, dW2 + rag);
    float3 earth = lerp(dirt, grav, gravM);
    float er = lerp(0.86 - rut * 0.08, 0.82, gravM);
    if (pathM > earthM) { earth = lerp(pathc, earth, saturate(earthM / max(pathM, 0.001))); }
    float allM = max(earthM, pathM);
    col = lerp(col, earth, allM);
    rough = lerp(rough, lerp(er, 0.9, strip * (1.0 - gravM)), allM);
  }
}
float dist = length(P.xy);
col = lerp(col, FarColor.rgb, smoothstep(FarStart, FarEnd, dist));
return float4(saturate(col), saturate(rough));
"""


def ground():
    m = fresh_material("M_DC_Ground")
    ca = vparam(m, "ColorA", (0.10, 0.20, 0.04, 1), -1200, -500)
    cb = vparam(m, "ColorB", (0.22, 0.24, 0.07, 1), -1200, -350)
    cc = vparam(m, "ColorC", (0.05, 0.12, 0.03, 1), -1200, -200)
    fc = vparam(m, "FarColor", (0.4, 0.45, 0.5, 1), -1200, -50)
    ins = [("P", expr(m, unreal.MaterialExpressionWorldPosition, -1200, 700)), ("ColorA", ca), ("ColorB", cb), ("ColorC", cc), ("FarColor", fc)]
    y = 50
    for nm, dv in (("Pattern", 0.0), ("Scale", 1.0), ("Rough", 0.9), ("Axis", 0.0), ("Center", 0.0), ("Width", 500.0),
                   ("FarStart", 14000.0), ("FarEnd", 26000.0)):
        ins.append((nm, sparam(m, "Roughness" if nm == "Rough" else nm, dv, -1200, y)))
        y += 80
    # site paint (Paint 1 on the meadow + lawn): yard rect, lane/driveway, footpath segments, gravel + dirt colours
    ins.append(("Paint", sparam(m, "Paint", 0.0, -1600, -500)))
    y = -400
    for nm, dv in (("SiteYard", (-1e6, -1e6, -1e6 + 1, -1e6 + 1)), ("SiteTrack", (-1e6, 1, -1e6, 1))) + tuple(
            (f"Seg{i}", (1e6, 1e6, 1e6, 1e6)) for i in range(7)) + (
            ("GravA", (0.30, 0.27, 0.22, 1)), ("GravB", (0.12, 0.105, 0.085, 1)),
            ("DirtA", (0.22, 0.16, 0.10, 1)), ("DirtB", (0.10, 0.07, 0.045, 1))):
        v = vparam(m, nm, dv, -1900, y)
        ap = expr(m, unreal.MaterialExpressionAppendVector, -1600, y)  # RGB + A -> float4
        if not (MEL.connect_material_expressions(v, "", ap, "A") and MEL.connect_material_expressions(v, "A", ap, "B")):
            unreal.log_warning(f"[DC] ground: failed to wire float4 for {nm}")
        ins.append((nm, ap))
        y += 90
    c = custom(m, GROUND_CODE, ins, out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4, desc="DCGround")
    MEL.connect_material_property(mask(m, c, True, True, True, False, -200, -200), "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(mask(m, c, False, False, False, True, -200, 0), "", MP.MP_ROUGHNESS)
    # Specular 0 = "soft" surface class: matte toy look, and M_DC_Toon keeps ink lines off grass and leaf facets
    sp0 = expr(m, unreal.MaterialExpressionConstant, -400, 420)
    sp0.set_editor_property("r", 0.0)
    MEL.connect_material_property(sp0, "", MP.MP_SPECULAR)
    return m


FOLIAGE_CODE = NOISE_LIB + r"""
float3 col = lerp(ColorA.rgb, ColorB.rgb, R);
float macro = d.fbm(float3(P.xy * 0.0006, 4.2));
col = lerp(col, ColorC.rgb, smoothstep(0.42, 0.75, macro) * CMix);
col = lerp(col, ColorC.rgb, saturate(VC.b) * 0.7); // leaf cards / bough tips tinted towards the accent colour
col *= lerp(AOMin, 1.12, saturate(VC.r));
return col;
"""

WIND_CODE = r"""
float ph = T * 1.6 + P.x * 0.011 + P.y * 0.007 + R * 6.2831;
float w = (sin(ph) + 0.45 * sin(ph * 2.3 + 1.7) + 0.25 * sin(T * 0.37 + P.y * 0.002)) * Wind * VC.g;
return float3(w, w * 0.55, -abs(w) * 0.15);
"""


def foliage():
    m = fresh_material("M_DC_Foliage")
    m.set_editor_property("two_sided", True)
    ca = vparam(m, "ColorA", (0.08, 0.20, 0.04, 1), -1200, -500)
    cb = vparam(m, "ColorB", (0.18, 0.26, 0.05, 1), -1200, -350)
    cc = vparam(m, "ColorC", (0.30, 0.22, 0.06, 1), -1200, -200)
    cm = sparam(m, "CMix", 0.4, -1200, -50)
    ao = sparam(m, "AOMin", 0.35, -1200, 30)
    wind = sparam(m, "Wind", 6.0, -1200, 110)
    rgh = sparam(m, "Roughness", 0.75, -600, 300)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1200, 200)
    vc = expr(m, unreal.MaterialExpressionVertexColor, -1200, 300)
    pr = expr(m, unreal.MaterialExpressionPerInstanceRandom, -1200, 400)
    t = expr(m, unreal.MaterialExpressionTime, -1200, 500)
    c = custom(m, FOLIAGE_CODE, [("P", wp), ("VC", vc), ("R", pr), ("ColorA", ca), ("ColorB", cb), ("ColorC", cc),
                                  ("CMix", cm), ("AOMin", ao)], desc="DCFoliage", y=-300)
    MEL.connect_material_property(c, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(rgh, "", MP.MP_ROUGHNESS)
    # Specular 0 = "soft" surface class: matte toy look, and M_DC_Toon keeps ink lines off grass and leaf facets
    sp0 = expr(m, unreal.MaterialExpressionConstant, -400, 420)
    sp0.set_editor_property("r", 0.0)
    MEL.connect_material_property(sp0, "", MP.MP_SPECULAR)
    # UpN 1 = shade as if facing straight up (grass blades take the ground's lighting instead of going dark side-on)
    upn = sparam(m, "UpN", 0.0, -1200, 600)
    upc = expr(m, unreal.MaterialExpressionConstant3Vector, -1200, 680)
    upc.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 0))
    upt = expr(m, unreal.MaterialExpressionTransform, -900, 680)
    upt.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    upt.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    MEL.connect_material_expressions(upc, "", upt, "")
    upl = expr(m, unreal.MaterialExpressionLinearInterpolate, -600, 640)
    MEL.connect_material_expressions(upc, "", upl, "A")
    # two-sided materials flip the shading normal on back faces: undo that for the up vector or half the blades go black
    ups = mul(m, upt, expr(m, unreal.MaterialExpressionTwoSidedSign, -900, 760), -750, 700)
    MEL.connect_material_expressions(ups, "", upl, "B")
    MEL.connect_material_expressions(upn, "", upl, "Alpha")
    MEL.connect_material_property(upl, "", MP.MP_NORMAL)
    w = custom(m, WIND_CODE, [("P", wp), ("VC", vc), ("R", pr), ("T", t), ("Wind", wind)], desc="DCWind", y=300)
    MEL.connect_material_property(w, "", MP.MP_WORLD_POSITION_OFFSET)
    return m


WATER_N = NOISE_LIB + r"""
float2 p = P.xy * 0.02;
float e = 0.35;
float t = T * 0.6;
float h0 = d.n(float3(p, t)) + 0.5 * d.n(float3(p * 2.7 + 3.0, t * 1.4));
float hx = d.n(float3(p + float2(e, 0), t)) + 0.5 * d.n(float3((p + float2(e, 0)) * 2.7 + 3.0, t * 1.4));
float hy = d.n(float3(p + float2(0, e), t)) + 0.5 * d.n(float3((p + float2(0, e)) * 2.7 + 3.0, t * 1.4));
return normalize(float3((h0 - hx) * Ripple, (h0 - hy) * Ripple, 1.0));
"""

WATER_E = NOISE_LIB + r"""
float sh = d.fbm(float3(P.xy * 0.004, T * 0.15));
return GlowColor.rgb * Glow * (0.45 + 0.9 * sh);
"""


def water():
    m = fresh_material("M_DC_Water")
    c = vparam(m, "Color", (0.015, 0.03, 0.035, 1), -900, -300)
    gc = vparam(m, "GlowColor", (0.05, 0.6, 0.55, 1), -900, -150)
    g = sparam(m, "Glow", 0.0, -900, 0)
    rp = sparam(m, "Ripple", 1.2, -900, 80)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -900, 160)
    t = expr(m, unreal.MaterialExpressionTime, -900, 240)
    MEL.connect_material_property(c, "", MP.MP_BASE_COLOR)
    r = expr(m, unreal.MaterialExpressionConstant, -400, 100)
    r.set_editor_property("r", 0.03)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    sp = expr(m, unreal.MaterialExpressionConstant, -400, 160)
    sp.set_editor_property("r", 0.6)
    MEL.connect_material_property(sp, "", MP.MP_SPECULAR)
    n = custom(m, WATER_N, [("P", wp), ("T", t), ("Ripple", rp)], desc="DCWaterN", y=300)
    MEL.connect_material_property(n, "", MP.MP_NORMAL)
    e = custom(m, WATER_E, [("P", wp), ("T", t), ("GlowColor", gc), ("Glow", g)], desc="DCWaterE", y=500)
    MEL.connect_material_property(e, "", MP.MP_EMISSIVE_COLOR)
    return m


def fixflags():
    # Wave-0 materials (Tools/create_materials.py) predate the ISM usage flag; set it in place so references stay intact.
    for name in ("M_DC_Emissive", "M_DC_Glass", "M_DC_Surface", "M_DC_Criminal"):
        mat = EAL.load_asset(f"{MAT_DIR}/{name}")
        if not mat:
            continue
        mat.set_editor_property("used_with_instanced_static_meshes", True)
        MEL.recompile_material(mat)
        EAL.save_loaded_asset(mat)
        unreal.log(f"[DC] flagged {name} for instanced meshes")
    return None


for f in (highlight, proc, neon, beam, rain, screen, window, fence, smoke, dust, glassi, ground, foliage, water, fixflags):
    if _only and f.__name__ not in _only:
        continue
    try:
        mat = f()
        if mat is None:
            continue
        MEL.recompile_material(mat)
        EAL.save_loaded_asset(mat)
        unreal.log(f"[DC] saved {mat.get_path_name()}")
    except Exception as e:  # keep going so one bad material doesn't block the rest
        unreal.log_error(f"[DC] material {f.__name__} failed: {e}")

unreal.log("[DC] scene_materials done")
