#include "eatscharted/chart_renderer.hpp"

#include <webgpu/webgpu.h>
#if !defined(__EMSCRIPTEN__)
#include <webgpu/wgpu.h>
#else
#include <emscripten.h>
#include <emscripten/html5.h>
#ifndef WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT
#define WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector static_cast<WGPUSType>(0x00040000)
typedef struct WGPUEmscriptenSurfaceSourceCanvasHTMLSelector {
    WGPUChainedStruct chain;
    WGPUStringView selector;
} WGPUEmscriptenSurfaceSourceCanvasHTMLSelector;
#endif
extern "C" {
WGPUDevice emscripten_webgpu_get_device(void);
}
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstdio>

namespace eatscharted {

namespace {

struct Mat4 {
    float m[16]{};

    static Mat4 identity() noexcept {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 multiply(const Mat4& a, const Mat4& b) noexcept {
        Mat4 r{};
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                r.m[c * 4 + row] =
                    a.m[0 * 4 + row] * b.m[c * 4 + 0] +
                    a.m[1 * 4 + row] * b.m[c * 4 + 1] +
                    a.m[2 * 4 + row] * b.m[c * 4 + 2] +
                    a.m[3 * 4 + row] * b.m[c * 4 + 3];
            }
        }
        return r;
    }

    static Mat4 lookAt(Vec3f eye, Vec3f center, Vec3f up) noexcept {
        Vec3f f{center.x - eye.x, center.y - eye.y, center.z - eye.z};
        float fLen = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
        if (fLen > 1e-6f) { f.x /= fLen; f.y /= fLen; f.z /= fLen; }

        Vec3f s{
            up.y * f.z - up.z * f.y,
            up.z * f.x - up.x * f.z,
            up.x * f.y - up.y * f.x
        };
        float sLen = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
        if (sLen > 1e-6f) { s.x /= sLen; s.y /= sLen; s.z /= sLen; }

        Vec3f u{
            f.y * s.z - f.z * s.y,
            f.z * s.x - f.x * s.z,
            f.x * s.y - f.y * s.x
        };

        Mat4 res = identity();
        res.m[0] = s.x;  res.m[4] = s.y;  res.m[8]  = s.z;
        res.m[1] = u.x;  res.m[5] = u.y;  res.m[9]  = u.z;
        res.m[2] = -f.x; res.m[6] = -f.y; res.m[10] = -f.z;
        res.m[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
        res.m[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
        res.m[14] =  (f.x * eye.x + f.y * eye.y + f.z * eye.z);
        return res;
    }

    static Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) noexcept {
        float f = 1.0f / std::tan(fovYRadians * 0.5f);
        Mat4 res{};
        res.m[0]  = f / aspect;
        res.m[5]  = f;
        res.m[10] = zFar / (zNear - zFar);
        res.m[11] = -1.0f;
        res.m[14] = (zFar * zNear) / (zNear - zFar);
        return res;
    }

    static Mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar) noexcept {
        Mat4 res = identity();
        res.m[0]  = 2.0f / (right - left);
        res.m[5]  = 2.0f / (top - bottom);
        res.m[10] = 1.0f / (zNear - zFar);
        res.m[12] = -(right + left) / (right - left);
        res.m[13] = -(top + bottom) / (top - bottom);
        res.m[14] = zNear / (zNear - zFar);
        return res;
    }
};

struct alignas(16) SceneUniforms {
    Mat4 viewProj;
    Mat4 lightViewProj;
    float camPos[4];     // xyz = camera position, w = timeSec
    float sunDir[4];     // xyz = normalized direction TO sun, w = fogDensity
    float sunColor[4];   // rgb = sun radiance, w = exposure
    float skyZenith[4];  // rgb = zenith color, w = focusDistance
    float skyHorizon[4]; // rgb = horizon color, w = focusRange
    float params[4];     // x = bokehRadius, y = width, z = height, w = horizonCurve
    float camBasisX[4];  // xyz = camera right, w = telegraphCount
    float camBasisY[4];  // xyz = camera up, w = pointLightCount
    float camBasisZ[4];  // xyz = camera forward, w = inkDensity
    float heroPos[4];    // xyz = hero world position, w = atlasZoom
};

// ============================================================================
// WGSL SHADER 0: SHADOW MAP PASS (3D Tabletop Relief + 2.5D Paper-Cutout Standees)
// ============================================================================
static const char* kShadowWgslSource = R"(
struct SceneUniforms {
    viewProj      : mat4x4<f32>,
    lightViewProj : mat4x4<f32>,
    camPos        : vec4<f32>,
    sunDir        : vec4<f32>,
    sunColor      : vec4<f32>,
    skyZenith     : vec4<f32>,
    skyHorizon    : vec4<f32>,
    params        : vec4<f32>,
    camBasisX     : vec4<f32>,
    camBasisY     : vec4<f32>,
    camBasisZ     : vec4<f32>,
    heroPos       : vec4<f32>,
};

struct ChartVertex {
    posAndCoast    : vec4<f32>,
    normAndRoad    : vec4<f32>,
    gradRiverBlend : vec4<f32>,
    uvBiomesFlags  : vec4<f32>,
};

struct StandeeInstance {
    posAndWidth : vec4<f32>,
    sizeAndMeta : vec4<f32>,
};

@group(0) @binding(0) var<uniform> u : SceneUniforms;
@group(0) @binding(1) var<storage, read> worldVerts : array<ChartVertex>;
@group(0) @binding(2) var<storage, read> standees   : array<StandeeInstance>;
@group(0) @binding(3) var albedoAtlas : texture_2d_array<f32>;

fn applyHorizonCurve(worldPos : vec3<f32>) -> vec3<f32> {
    let dz = max(0.0, (worldPos.z - u.heroPos.z) - 8.0);
    let dx = (worldPos.x - u.heroPos.x) * 0.35;
    let curveDrop = (dz * dz + dx * dx) * u.params.w;
    return vec3<f32>(worldPos.x, worldPos.y - curveDrop, worldPos.z);
}

@vertex
fn vs_shadow_world(@builtin(vertex_index) vertexIndex : u32) -> @builtin(position) vec4<f32> {
    let v = worldVerts[vertexIndex];
    let p = applyHorizonCurve(v.posAndCoast.xyz);
    return u.lightViewProj * vec4<f32>(p, 1.0);
}

struct StandeeShadowOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) uv            : vec2<f32>,
    @location(1) @interpolate(flat) texLayer : u32,
};

@vertex
fn vs_shadow_sprite(@builtin(vertex_index) vertexIndex : u32) -> StandeeShadowOut {
    let spriteIdx = vertexIndex / 6u;
    let cornerIdx = vertexIndex % 6u;
    let sp = standees[spriteIdx];

    let basePos = applyHorizonCurve(sp.posAndWidth.xyz);
    let width   = sp.posAndWidth.w;
    let height  = sp.sizeAndMeta.x;
    let layer   = bitcast<u32>(sp.sizeAndMeta.y);
    let flags   = bitcast<u32>(sp.sizeAndMeta.z);
    let phase   = sp.sizeAndMeta.w;

    var corners = array<vec2<f32>, 6>(
        vec2<f32>(-0.5, 0.0),
        vec2<f32>( 0.5, 0.0),
        vec2<f32>( 0.5, 1.0),
        vec2<f32>(-0.5, 0.0),
        vec2<f32>( 0.5, 1.0),
        vec2<f32>(-0.5, 1.0)
    );
    let c = corners[cornerIdx];

    var uCoord = c.x + 0.5;
    if ((flags & 1u) != 0u) {
        uCoord = 1.0 - uCoord;
    }
    let uv = vec2<f32>(uCoord, 1.0 - c.y);

    let time = u.camPos.w;
    var swayX = 0.0;
    var bobY  = 0.0;
    if ((flags & 2u) != 0u) {
        swayX = sin(time * 1.7 + phase) * 0.11 * c.y;
    }
    if ((flags & 4u) != 0u) {
        bobY = abs(sin(time * 2.8 + phase)) * 0.07 * c.y;
    }

    let rightVec = normalize(vec3<f32>(u.camBasisX.x, 0.0, u.camBasisX.z));
    let worldPos = basePos + rightVec * (c.x * width + swayX) + vec3<f32>(0.0, c.y * height + bobY, 0.0);

    var out : StandeeShadowOut;
    out.clipPos  = u.lightViewProj * vec4<f32>(worldPos, 1.0);
    out.uv       = uv;
    out.texLayer = layer;
    return out;
}

@fragment
fn fs_shadow_sprite(in : StandeeShadowOut) {
    let texCoord = clamp(vec2<i32>(in.uv * 64.0), vec2<i32>(0, 0), vec2<i32>(63, 63));
    let sampleCol = textureLoad(albedoAtlas, texCoord, i32(in.texLayer), 0);
    if (sampleCol.a < 0.5) {
        discard;
    }
}
)";

// ============================================================================
// WGSL SHADER 1: LIVING PARCHMENT CARTOGRAPHY & 2.5D STANDEE SCENE PASS
// ============================================================================
static const char* kSceneWgslPart1 = R"(
struct SceneUniforms {
    viewProj      : mat4x4<f32>,
    lightViewProj : mat4x4<f32>,
    camPos        : vec4<f32>,
    sunDir        : vec4<f32>,
    sunColor      : vec4<f32>,
    skyZenith     : vec4<f32>,
    skyHorizon    : vec4<f32>,
    params        : vec4<f32>,
    camBasisX     : vec4<f32>,
    camBasisY     : vec4<f32>,
    camBasisZ     : vec4<f32>,
    heroPos       : vec4<f32>,
};

struct ChartVertex {
    posAndCoast    : vec4<f32>,
    normAndRoad    : vec4<f32>,
    gradRiverBlend : vec4<f32>,
    uvBiomesFlags  : vec4<f32>,
};

struct StandeeInstance {
    posAndWidth : vec4<f32>,
    sizeAndMeta : vec4<f32>,
};

struct PointLight {
    posAndRadius : vec4<f32>,
    colAndInt    : vec4<f32>,
};

struct CombatTelegraph {
    centerAndRadius : vec4<f32>,
    dirProgType     : vec4<f32>,
};

@group(0) @binding(0) var<uniform> u : SceneUniforms;
@group(0) @binding(1) var<storage, read> worldVerts  : array<ChartVertex>;
@group(0) @binding(2) var<storage, read> standees    : array<StandeeInstance>;
@group(0) @binding(3) var<storage, read> pointLights : array<PointLight>;
@group(0) @binding(4) var<storage, read> telegraphs  : array<CombatTelegraph>;
@group(0) @binding(5) var albedoAtlas   : texture_2d_array<f32>;
@group(0) @binding(6) var normalAtlas   : texture_2d_array<f32>;
@group(0) @binding(7) var atlasSampler  : sampler;
@group(0) @binding(8) var shadowMap     : texture_depth_2d;
@group(0) @binding(9) var shadowSampler : sampler_comparison;

fn hash2D(p : vec2<f32>) -> f32 {
    let h = dot(p, vec2<f32>(127.1, 311.7));
    return fract(sin(h) * 43758.5453123);
}

fn valueNoise2D(p : vec2<f32>) -> f32 {
    let i = floor(p);
    let f = fract(p);
    let u2 = f * f * (3.0 - 2.0 * f);
    let a = hash2D(i);
    let b = hash2D(i + vec2<f32>(1.0, 0.0));
    let c = hash2D(i + vec2<f32>(0.0, 1.0));
    let d = hash2D(i + vec2<f32>(1.0, 1.0));
    return mix(mix(a, b, u2.x), mix(c, d, u2.x), u2.y);
}

fn fbmPaper(p : vec2<f32>) -> f32 {
    var v = 0.0;
    var a = 0.5;
    var q = p;
    for (var i = 0; i < 4; i = i + 1) {
        v += a * valueNoise2D(q);
        q = q * 2.03 + vec2<f32>(17.3, 9.2);
        a *= 0.5;
    }
    return v;
}

fn computeCrispUV(uv : vec2<f32>, fw : vec2<f32>) -> vec2<f32> {
    let texSize = 64.0;
    let pixel = uv * texSize;
    let safeFw = max(fw * texSize, vec2<f32>(0.001, 0.001));
    let seam = floor(pixel + 0.5);
    let frac = clamp((pixel - seam) / safeFw + 0.5, vec2<f32>(0.0), vec2<f32>(1.0));
    return (seam - 0.5 + frac) / texSize;
}

fn applyHorizonCurve(worldPos : vec3<f32>) -> vec3<f32> {
    let dz = max(0.0, (worldPos.z - u.heroPos.z) - 8.0);
    let dx = (worldPos.x - u.heroPos.x) * 0.35;
    let curveDrop = (dz * dz + dx * dx) * u.params.w;
    return vec3<f32>(worldPos.x, worldPos.y - curveDrop, worldPos.z);
}

fn sampleShadowPCF(worldPos : vec3<f32>) -> f32 {
    let lightClip = u.lightViewProj * vec4<f32>(worldPos, 1.0);
    let ndc = lightClip.xyz / lightClip.w;
    let shadowUV = vec2<f32>(ndc.x * 0.5 + 0.5, -ndc.y * 0.5 + 0.5);
    if (shadowUV.x < 0.01 || shadowUV.x > 0.99 || shadowUV.y < 0.01 || shadowUV.y > 0.99 || ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }
    let texel = 1.0 / 2048.0;
    let depthRef = ndc.z - 0.0025;
    var sum = 0.0;
    for (var y = -1; y <= 1; y = y + 1) {
        for (var x = -1; x <= 1; x = x + 1) {
            let offset = vec2<f32>(f32(x), f32(y)) * texel;
            sum += textureSampleCompare(shadowMap, shadowSampler, shadowUV + offset, depthRef);
        }
    }
    return sum / 9.0;
}

// Sky / Antique Vellum Backdrop Pass
struct SkyOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) uv            : vec2<f32>,
};

@vertex
fn vs_sky(@builtin(vertex_index) vertexIndex : u32) -> SkyOut {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 3.0, -1.0),
        vec2<f32>(-1.0,  3.0)
    );
    var out : SkyOut;
    let p = pos[vertexIndex];
    out.clipPos = vec4<f32>(p, 0.9999, 1.0);
    out.uv = vec2<f32>(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    return out;
}

@fragment
fn fs_sky(in : SkyOut) -> @location(0) vec4<f32> {
    let t = clamp(in.uv.y, 0.0, 1.0);
    let grain = (fbmPaper(in.uv * 48.0) - 0.5) * 0.04;
    let skyCol = mix(u.skyZenith.rgb, u.skyHorizon.rgb, pow(t, 0.75)) + vec3<f32>(grain);
    return vec4<f32>(skyCol, 240.0);
}

// Watabou Perilous Shores + Cat Quest Biome Watercolor Palette
fn biomePaperTint(biomeId : u32) -> vec3<f32> {
    switch (biomeId) {
        case 0u: { return vec3<f32>(0.68, 0.76, 0.74); } // OceanDeep (parchment-tinted nautical chart sea)
        case 1u: { return vec3<f32>(0.76, 0.82, 0.76); } // CoastalShoals (pale turquoise-vellum)
        case 2u: { return vec3<f32>(0.90, 0.83, 0.67); } // SandyBeach (warm stippled sand)
        case 3u: { return vec3<f32>(0.66, 0.72, 0.56); } // SaltMarsh (reed olive wash)
        case 4u: { return vec3<f32>(0.77, 0.80, 0.62); } // LushMeadow (sage-vellum meadow)
        case 5u: { return vec3<f32>(0.67, 0.74, 0.54); } // DeciduousWoodland (moss wash)
        case 6u: { return vec3<f32>(0.64, 0.71, 0.63); } // TaigaPines (cool highland slate-sage)
        case 7u: { return vec3<f32>(0.86, 0.77, 0.58); } // AridSteppe (ochre savanna)
        case 8u: { return vec3<f32>(0.82, 0.68, 0.54); } // BadlandsCanyon (terracotta sienna)
        case 9u: { return vec3<f32>(0.85, 0.81, 0.73); } // AlpinePeak (sunlit stone parchment)
        case 10u:{ return vec3<f32>(0.58, 0.52, 0.60); } // CalamityBlight (ashen violet)
        case 11u:{ return vec3<f32>(0.84, 0.79, 0.69); } // DungeonFlagstone (carved parchment floor)
        case 12u:{ return vec3<f32>(0.72, 0.66, 0.56); } // DungeonHatchedWall (cross-hatched rock canvas)
        default: { return vec3<f32>(0.84, 0.78, 0.66); }
    }
}

struct WorldVSOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) worldPos      : vec3<f32>,
    @location(1) rawMapPos     : vec3<f32>,
    @location(2) normal        : vec3<f32>,
    @location(3) sdfs          : vec4<f32>, // x = sdfCoast, y = sdfRoad, z = sdfRiver, w = biomeBlend
    @location(4) gradAndUV     : vec4<f32>, // xy = gradXZ, z = roadArc, w = riverWidth
    @location(5) @interpolate(flat) packedBiomes : u32,
    @location(6) @interpolate(flat) flags        : u32,
};

@vertex
fn vs_world(@builtin(vertex_index) vertexIndex : u32) -> WorldVSOut {
    let v = worldVerts[vertexIndex];
    let rawPos = v.posAndCoast.xyz;
    let curvedPos = applyHorizonCurve(rawPos);

    var out : WorldVSOut;
    out.clipPos      = u.viewProj * vec4<f32>(curvedPos, 1.0);
    out.worldPos     = curvedPos;
    out.rawMapPos    = rawPos;
    out.normal       = v.normAndRoad.xyz;
    out.sdfs         = vec4<f32>(v.posAndCoast.w, v.normAndRoad.w, v.gradRiverBlend.z, v.gradRiverBlend.w);
    out.gradAndUV    = vec4<f32>(v.gradRiverBlend.xy, v.uvBiomesFlags.xy);
    out.packedBiomes = bitcast<u32>(v.uvBiomesFlags.z);
    out.flags        = bitcast<u32>(v.uvBiomesFlags.w);
    return out;
}
)";

static const char* kSceneWgslPart2 = R"(
@fragment
fn fs_world(in : WorldVSOut) -> @location(0) vec4<f32> {
    let inkColor    = vec3<f32>(0.13, 0.09, 0.07);
    let inkSoft     = vec3<f32>(0.28, 0.21, 0.16);
    let mapXZ       = in.rawMapPos.xz;
    let time        = u.camPos.w;
    let sdfCoast    = in.sdfs.x;
    let sdfRoad     = in.sdfs.y;
    let sdfRiver    = in.sdfs.z;
    let biomeBlend  = in.sdfs.w;
    let gradXZ      = in.gradAndUV.xy;
    let roadArc     = in.gradAndUV.z;

    let biome0 = in.packedBiomes & 255u;
    let biome1 = (in.packedBiomes >> 8u) & 255u;

    // 0. Check if this vertex belongs to a 3D Curved Calligraphic Map Banner!
    if ((in.flags & 8u) != 0u) {
        let bannerInk = select(vec3<f32>(0.14, 0.09, 0.06), vec3<f32>(0.20, 0.28, 0.36), biome0 == 0u);
        let camDist = length(in.worldPos - u.camPos.xyz);
        return vec4<f32>(bannerInk, camDist);
    }

    // 1. Base Fibrous Parchment Grain & Biome Watercolor Wash
    let paperFiber = fbmPaper(mapXZ * 1.45);
    let fineGrain  = valueNoise2D(mapXZ * 8.5);
    var col = biomePaperTint(biome0);

    // Watercolor pigment pooling at biome edges
    if (biome0 != biome1 && biomeBlend > 0.2) {
        let edgePool = smoothstep(0.2, 0.9, biomeBlend) * 0.11;
        col -= vec3<f32>(edgePool, edgePool * 1.05, edgePool * 1.15);
    }
    col *= (0.93 + 0.12 * paperFiber + 0.04 * (fineGrain - 0.5));

    // Subtle cartographic lat/long & compass rhumb grid across the map
    let gridX = abs(fract(mapXZ.x / 16.0 + 0.5) - 0.5) * 16.0;
    let gridZ = abs(fract(mapXZ.y / 16.0 + 0.5) - 0.5) * 16.0;
    let gridLine = 1.0 - smoothstep(0.0, 0.08, min(gridX, gridZ));
    col = mix(col, inkSoft, gridLine * 0.14);

    // 2. Watabou One-Page Dungeon Mode (Cross-Hatched Walls & Gridded Flagstones)
    if ((in.flags & 16u) != 0u) {
        if (biome0 == 12u) {
            // Watabou signature dense diagonal cross-hatching on solid dungeon rock perimeter
            let h1 = abs(fract((mapXZ.x + mapXZ.y) * 1.85) - 0.5);
            let h2 = abs(fract((mapXZ.x - mapXZ.y) * 1.85) - 0.5);
            let hatch1 = 1.0 - smoothstep(0.08, 0.18, h1);
            let wallBorderDist = -sdfCoast;
            let denseNearWall = smoothstep(3.8, 0.0, wallBorderDist);
            let hatch2 = (1.0 - smoothstep(0.08, 0.18, h2)) * denseNearWall;
            let wallStroke = smoothstep(0.32, 0.08, wallBorderDist);
            let inkW = clamp(hatch1 * 0.65 + hatch2 * 0.55 + wallStroke, 0.0, 1.0);
            col = mix(col, inkColor, inkW);
        } else if (biome0 == 11u) {
            // 2.0m Dungeon Flagstone Grid with double-inked mortar seams & corner bevel ticks
            let tileX = abs(fract(mapXZ.x * 0.5 + 0.5) - 0.5) * 2.0;
            let tileZ = abs(fract(mapXZ.y * 0.5 + 0.5) - 0.5) * 2.0;
            let seam = 1.0 - smoothstep(0.0, 0.06, min(tileX, tileZ));
            let wallShadow = exp(-sdfCoast * 1.1) * 0.28;
            col = mix(col * (1.0 - wallShadow), inkSoft, seam * 0.45);
        }
    } else {
        // 3. Overworld Cartography: Ocean Concentric Shoreline Water-Lining vs. Land Hachures
        let jitter = (paperFiber - 0.5) * 0.35;
        let coastD = sdfCoast + jitter;

        if (coastD < 0.0) {
            // OCEAN / BAY: Watabou's signature Concentric Shoreline Water-Lining Rings!
            let shoreDist = -coastD;
            let waveAnim  = sin(time * 1.35 + mapXZ.x * 0.25 + mapXZ.y * 0.20) * 0.12;
            let ringFreq  = 0.82;
            let ringPhase = fract((shoreDist + waveAnim) * ringFreq);
            let ringLine  = 1.0 - smoothstep(0.0, 0.16, abs(ringPhase - 0.35));
            let shoreFade = exp(-shoreDist * 0.19) * smoothstep(0.15, 0.55, shoreDist);

            // Shallow coastal stipple dots
            let stipple = step(0.84, hash2D(floor(mapXZ * 4.5))) * exp(-shoreDist * 0.45);

            // Deep-water horizontal wave-dash clusters
            let waveRow = abs(fract(mapXZ.y * 0.38) - 0.5);
            let waveSeg = fract(mapXZ.x * 0.14 + floor(mapXZ.y * 0.38) * 0.37);
            let deepDash = (1.0 - smoothstep(0.0, 0.06, waveRow)) * step(0.68, waveSeg) * smoothstep(4.0, 10.0, shoreDist);

            // Compass rhumb diagonals in deep ocean
            let rhumb1 = 1.0 - smoothstep(0.0, 0.05, abs(fract((mapXZ.x + mapXZ.y * 1.732) / 28.0) - 0.5) * 28.0);
            let rhumb2 = 1.0 - smoothstep(0.0, 0.05, abs(fract((mapXZ.x - mapXZ.y * 1.732) / 28.0) - 0.5) * 28.0);
            let rhumbMask = max(rhumb1, rhumb2) * smoothstep(5.0, 14.0, shoreDist) * 0.18;

            let waterInk = clamp(ringLine * shoreFade * 0.78 + stipple * 0.45 + deepDash * 0.32 + rhumbMask, 0.0, 1.0);
            col = mix(col, inkSoft, waterInk);
        } else {
            // LANDMASS:
            // A) Beach sand stippling near shoreline
            if (coastD < 2.6) {
                let sandDots = step(0.80, hash2D(floor(mapXZ * 5.0))) * (1.0 - coastD / 2.6);
                col = mix(col, inkSoft, sandDots * 0.42);
            }

            // B) Salt Marsh horizontal grass-tuft ticks
            if (biome0 == 3u) {
                let tuftRow = abs(fract(mapXZ.y * 1.2) - 0.5);
                let tuftSeg = fract(mapXZ.x * 0.65 + floor(mapXZ.y * 1.2) * 0.41);
                let marshTick = (1.0 - smoothstep(0.0, 0.09, tuftRow)) * step(0.72, tuftSeg);
                col = mix(col, inkSoft, marshTick * 0.48);
            }

            // C) Watabou Topographic Slope Hachures aligned with downhill gradient (∇h)!
            let slopeMag = length(gradXZ);
            if (slopeMag > 0.16) {
                let downDir = gradXZ / max(slopeMag, 0.001);
                let tangDir = vec2<f32>(-downDir.y, downDir.x);
                let uAlongContour = dot(mapXZ, tangDir);
                let vDownSlope    = dot(mapXZ, downDir);

                // Tapered hachure pen strokes running parallel to downhill slope
                let tickSpacing = abs(fract(uAlongContour * 1.65 + valueNoise2D(mapXZ * 0.5) * 0.25) - 0.5);
                let tickBand    = fract(vDownSlope * 0.55);
                let taperWidth  = 0.14 * (1.0 - abs(tickBand - 0.5) * 1.6);
                let hachureLine = 1.0 - smoothstep(max(0.02, taperWidth * 0.5), max(0.06, taperWidth), tickSpacing);
                let slopeWeight = smoothstep(0.16, 0.55, slopeMag) * 0.68;
                col = mix(col, inkColor, hachureLine * slopeWeight);
            }

            // D) Winding Rivers with inked banks
            if (sdfRiver < 0.25) {
                let riverFill = 1.0 - smoothstep(-0.15, 0.10, sdfRiver);
                let riverBank = 1.0 - smoothstep(0.0, 0.18, abs(sdfRiver));
                let riverCol  = vec3<f32>(0.64, 0.75, 0.74);
                col = mix(col, riverCol, riverFill);
                col = mix(col, inkColor, riverBank * 0.82);
            }

            // E) Spline Trade Roads (Royal double-cased highways vs. Dashed wilderness trails)
            if ((in.flags & 2u) != 0u) {
                // Timber / Stone Bridge crossing river
                let plank = abs(fract(roadArc * 1.8) - 0.5);
                let bridgeCol = mix(vec3<f32>(0.76, 0.62, 0.44), inkColor, step(0.42, plank) * 0.45);
                col = mix(col, bridgeCol, 1.0 - smoothstep(0.7, 1.1, sdfRoad));
            } else if (sdfRoad < 1.05) {
                if (roadArc >= 0.0) {
                    // Royal Cased Highway: warm buff ribbon + twin inked casing edges
                    let roadFill = 1.0 - smoothstep(0.55, 0.85, sdfRoad);
                    let roadEdge = 1.0 - smoothstep(0.0, 0.16, abs(sdfRoad - 0.68));
                    col = mix(col, vec3<f32>(0.91, 0.84, 0.68), roadFill * 0.78);
                    col = mix(col, inkColor, roadEdge * 0.72);
                } else {
                    // Dashed Explorer Trail
                    let dashMask = step(0.42, fract(abs(roadArc) * 0.55));
                    let trailStroke = (1.0 - smoothstep(0.12, 0.34, sdfRoad)) * dashMask;
                    col = mix(col, inkColor, trailStroke * 0.76);
                }
            }
        }

        // Bold India-ink coastline contour at sdfCoast == 0
        let coastContour = 1.0 - smoothstep(0.0, 0.24, abs(coastD));
        col = mix(col, inkColor, coastContour * 0.90);
    }

    // 4. Diegetic Map-Surface Combat Telegraphs (Cat Quest AoE Circles, Cones & Arcane Rings!)
    let teleCount = u32(u.camBasisX.w);
    for (var ti = 0u; ti < teleCount; ti = ti + 1u) {
        let tg = telegraphs[ti];
        let delta = mapXZ - tg.centerAndRadius.xz;
        let dist  = length(delta);
        let rad   = max(tg.centerAndRadius.w, 0.1);
        if (dist <= rad * 1.08) {
            let prog  = clamp(tg.dirProgType.z, 0.0, 1.0);
            let tType = bitcast<u32>(tg.dirProgType.w);

            var inShape = 1.0;
            if (tType == 1u) {
                // Directional 75-degree Enemy Cone Telegraph
                let dir2 = normalize(tg.dirProgType.xy);
                let cosAng = dot(delta / max(dist, 0.001), dir2);
                inShape = smoothstep(0.74, 0.80, cosAng);
            }

            if (inShape > 0.01) {
                let outerRing = 1.0 - smoothstep(0.0, 0.16, abs(dist - rad));
                let innerFill = step(dist, rad * prog) * 0.36 + step(dist, rad) * 0.16;

                if (tType == 0u || tType == 1u) {
                    // Crimson Enemy Telegraph
                    let dangerCol = vec3<f32>(0.92, 0.22, 0.18);
                    col = mix(col, dangerCol, (innerFill + outerRing * 0.75) * inShape);
                } else if (tType == 2u) {
                    // Sapphire/Gold Player Arcane Spell Circle
                    let arcaneCol = vec3<f32>(0.24, 0.72, 1.0);
                    let runeRing  = 1.0 - smoothstep(0.0, 0.14, abs(dist - rad * 0.68));
                    col = mix(col, arcaneCol, (innerFill * 0.8 + (outerRing + runeRing) * 0.75));
                } else {
                    // Golden Cartographer Quest / Portal Compass Ring
                    let goldCol = vec3<f32>(0.95, 0.76, 0.24);
                    col = mix(col, goldCol, outerRing * (0.55 + 0.35 * prog));
                }
            }
        }
    }

    // 5. Directional Sunlight, Shadow Map & Dynamic Point Lights on the Parchment Relief
    let N = normalize(in.normal);
    let L = normalize(u.sunDir.xyz);
    let ndotl = max(dot(N, L), 0.0);
    let shadow = sampleShadowPCF(in.worldPos);

    // Warm cartographic paper lighting (high ambient so map ink remains crystal-clear)
    var lit = col * (vec3<f32>(0.66, 0.63, 0.58) + u.sunColor.rgb * (0.38 * ndotl * shadow));

    let lightCount = u32(u.camBasisY.w);
    for (var li = 0u; li < lightCount; li = li + 1u) {
        let pl = pointLights[li];
        let toL = pl.posAndRadius.xyz - in.rawMapPos;
        let d   = length(toL);
        let r   = pl.posAndRadius.w;
        if (d < r) {
            let atten = pow(1.0 - d / r, 2.0) * pl.colAndInt.w;
            lit += col * pl.colAndInt.rgb * atten * 0.42;
        }
    }

    let camDist = length(in.worldPos - u.camPos.xyz);
    let fogFactor = clamp(1.0 - exp(-camDist * u.sunDir.w), 0.0, 0.65);
    lit = mix(lit, u.skyHorizon.rgb, fogFactor);

    return vec4<f32>(lit, camDist);
}

// 2.5D Inked Paper-Cutout Standees Pass
struct StandeeVSOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) worldPos      : vec3<f32>,
    @location(1) rawFootPos    : vec3<f32>,
    @location(2) uv            : vec2<f32>,
    @location(3) @interpolate(flat) texLayer : u32,
    @location(4) @interpolate(flat) flags    : u32,
};

@vertex
fn vs_sprite(@builtin(vertex_index) vertexIndex : u32) -> StandeeVSOut {
    let spriteIdx = vertexIndex / 6u;
    let cornerIdx = vertexIndex % 6u;
    let sp = standees[spriteIdx];

    let rawFoot  = sp.posAndWidth.xyz;
    let basePos  = applyHorizonCurve(rawFoot);
    let width    = sp.posAndWidth.w;
    let height   = sp.sizeAndMeta.x;
    let layer    = bitcast<u32>(sp.sizeAndMeta.y);
    let flags    = bitcast<u32>(sp.sizeAndMeta.z);
    let phase    = sp.sizeAndMeta.w;

    var corners = array<vec2<f32>, 6>(
        vec2<f32>(-0.5, 0.0),
        vec2<f32>( 0.5, 0.0),
        vec2<f32>( 0.5, 1.0),
        vec2<f32>(-0.5, 0.0),
        vec2<f32>( 0.5, 1.0),
        vec2<f32>(-0.5, 1.0)
    );
    let c = corners[cornerIdx];

    var uCoord = c.x + 0.5;
    if ((flags & 1u) != 0u) {
        uCoord = 1.0 - uCoord;
    }
    let uv = vec2<f32>(uCoord, 1.0 - c.y);

    let time = u.camPos.w;
    var swayX = 0.0;
    var bobY  = 0.0;
    if ((flags & 2u) != 0u) {
        swayX = sin(time * 1.7 + phase) * 0.11 * c.y;
    }
    if ((flags & 4u) != 0u) {
        bobY = abs(sin(time * 2.8 + phase)) * 0.08 * c.y;
    }

    // Proximity Canopy Parting: gently lean trees/mountains away when hero walks behind them!
    if ((flags & 16u) != 0u) {
        let toStandee = rawFoot.xz - u.heroPos.xz;
        let distH = length(toStandee);
        if (distH < 4.2 && rawFoot.z < u.heroPos.z + 0.5) {
            let pushDir = select(-1.0, 1.0, toStandee.x >= 0.0);
            swayX += pushDir * (1.0 - distH / 4.2) * 0.65 * c.y;
        }
    }

    let rightVec = normalize(vec3<f32>(u.camBasisX.x, 0.0, u.camBasisX.z));
    // Slight backward tabletop standee tilt for 2.5D pop-up feel
    let tiltZ = select(0.0, 0.16 * c.y * height, (flags & 32u) != 0u);
    let worldPos = basePos + rightVec * (c.x * width + swayX) + vec3<f32>(0.0, c.y * height + bobY, tiltZ);

    var out : StandeeVSOut;
    out.clipPos    = u.viewProj * vec4<f32>(worldPos, 1.0);
    out.worldPos   = worldPos;
    out.rawFootPos = rawFoot;
    out.uv         = uv;
    out.texLayer   = layer;
    out.flags      = flags;
    return out;
}

@fragment
fn fs_sprite(in : StandeeVSOut) -> @location(0) vec4<f32> {
    let fw = fwidth(in.uv);
    let crispUV = computeCrispUV(in.uv, fw);
    let albedo = textureSampleLevel(albedoAtlas, atlasSampler, crispUV, i32(in.texLayer), 0.0);
    if (albedo.a < 0.5) {
        discard;
    }

    // Screen-door stipple transparency if hero walks directly behind a tall canopy/mountain standee
    if ((in.flags & 16u) != 0u) {
        let distH = length(in.rawFootPos.xz - u.heroPos.xz);
        if (distH < 3.2 && in.rawFootPos.z < u.heroPos.z) {
            let checker = fract(in.clipPos.x * 0.5 + in.clipPos.y * 0.5);
            if (checker < 0.45) {
                discard;
            }
        }
    }

    let normSample = textureSampleLevel(normalAtlas, atlasSampler, crispUV, i32(in.texLayer), 0.0);
    let emissive = normSample.a;

    let shadow = sampleShadowPCF(in.worldPos);
    var lit = albedo.rgb * (vec3<f32>(0.68, 0.64, 0.58) + u.sunColor.rgb * (0.42 * shadow));

    if ((in.flags & 8u) != 0u || emissive > 0.05) {
        lit = mix(lit, albedo.rgb * 1.45, max(emissive, 0.65));
    }

    // Combat Hit Flash (white/gold flash when struck by hero's blade or spell)
    if ((in.flags & 64u) != 0u) {
        lit = mix(lit, vec3<f32>(1.6, 1.4, 1.1), 0.72);
    }

    let camDist = length(in.worldPos - u.camPos.xyz);
    let fogFactor = clamp(1.0 - exp(-camDist * u.sunDir.w), 0.0, 0.65);
    lit = mix(lit, u.skyHorizon.rgb, fogFactor);

    return vec4<f32>(lit, camDist);
}
)";

// ============================================================================
// WGSL SHADER 2: TABLETOP TILT-SHIFT BOKEH & ANTIQUE VELLUM POST-PROCESS PASS
// ============================================================================
static const char* kPostProcessWgslSource = R"(
struct SceneUniforms {
    viewProj      : mat4x4<f32>,
    lightViewProj : mat4x4<f32>,
    camPos        : vec4<f32>,
    sunDir        : vec4<f32>,
    sunColor      : vec4<f32>,
    skyZenith     : vec4<f32>,
    skyHorizon    : vec4<f32>,
    params        : vec4<f32>,
    camBasisX     : vec4<f32>,
    camBasisY     : vec4<f32>,
    camBasisZ     : vec4<f32>,
    heroPos       : vec4<f32>,
};

@group(0) @binding(0) var<uniform> u       : SceneUniforms;
@group(0) @binding(1) var sceneTex         : texture_2d<f32>;
@group(0) @binding(2) var sceneSampler     : sampler;

struct PostVSOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) uv            : vec2<f32>,
};

@vertex
fn vs_post(@builtin(vertex_index) vertexIndex : u32) -> PostVSOut {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 3.0, -1.0),
        vec2<f32>(-1.0,  3.0)
    );
    var out : PostVSOut;
    let p = pos[vertexIndex];
    out.clipPos = vec4<f32>(p, 0.0, 1.0);
    out.uv = vec2<f32>(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    return out;
}

fn acesFilmic(x : vec3<f32>) -> vec3<f32> {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3<f32>(0.0), vec3<f32>(1.0));
}

@fragment
fn fs_post(in : PostVSOut) -> @location(0) vec4<f32> {
    let centerSample = textureSampleLevel(sceneTex, sceneSampler, in.uv, 0.0);
    let dist = centerSample.a;

    let focusDist  = u.skyZenith.w;
    let focusRange = max(u.skyHorizon.w, 1.0);
    let maxBokeh   = u.params.x;

    // Gentle tabletop tilt-shift at the upper horizon and extreme foreground
    let topTilt = smoothstep(0.24, 0.02, in.uv.y) * 0.55;
    let depthCoc = clamp((abs(dist - focusDist) - focusRange) / focusRange, 0.0, 1.0);
    let coc = clamp(max(depthCoc * 0.65, topTilt), 0.0, 1.0) * maxBokeh;

    let texelSize = 1.0 / vec2<f32>(max(u.params.y, 1.0), max(u.params.z, 1.0));
    var accum = centerSample.rgb;
    var wSum  = 1.0;

    if (coc > 0.35) {
        let goldenAngle = 2.39996323;
        for (var i = 1; i <= 16; i = i + 1) {
            let r = sqrt(f32(i) / 16.0) * coc;
            let theta = f32(i) * goldenAngle;
            let offset = vec2<f32>(cos(theta), sin(theta)) * r * texelSize;
            let tap = textureSampleLevel(sceneTex, sceneSampler, clamp(in.uv + offset, vec2<f32>(0.001), vec2<f32>(0.999)), 0.0);
            accum += tap.rgb;
            wSum  += 1.0;
        }
    }

    var hdr = (accum / wSum) * u.sunColor.w;

    // Antique Cartographer's Parchment Border Vignette
    let centered = in.uv - vec2<f32>(0.5, 0.5);
    let vig = 1.0 - dot(centered, centered) * 0.34;
    hdr *= vig;

    var mapped = acesFilmic(hdr);
    mapped = pow(mapped, vec3<f32>(1.0 / 2.2));
    return vec4<f32>(mapped, 1.0);
}
)";

// ============================================================================
// WGSL SHADER 3: EATSBITS 2D VECTOR & TYPOGRAPHY OVERLAY PASS (PASS 4)
// ============================================================================
static const char* kOverlay2dWgslSource = R"(
struct SceneUniforms {
    viewProj      : mat4x4<f32>,
    lightViewProj : mat4x4<f32>,
    camPos        : vec4<f32>,
    sunDir        : vec4<f32>,
    sunColor      : vec4<f32>,
    skyZenith     : vec4<f32>,
    skyHorizon    : vec4<f32>,
    params        : vec4<f32>,
    camBasisX     : vec4<f32>,
    camBasisY     : vec4<f32>,
    camBasisZ     : vec4<f32>,
    heroPos       : vec4<f32>,
};

struct OverlayVertex2D {
    pos   : vec2<f32>,
    uv    : vec2<f32>,
    color : u32,
    mode  : u32,
    pad   : vec2<f32>,
};

@group(0) @binding(0) var<uniform> u : SceneUniforms;
@group(0) @binding(1) var<storage, read> overlayVerts : array<OverlayVertex2D>;

struct OverlayVSOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) uv            : vec2<f32>,
    @location(1) color         : vec4<f32>,
    @location(2) @interpolate(flat) mode : u32,
    @location(3) localPos      : vec2<f32>,
    @location(4) @interpolate(flat) halfSize : vec2<f32>,
    @location(5) @interpolate(flat) radius   : f32,
};

@vertex
fn vs_overlay2d(@builtin(vertex_index) vertexIndex : u32) -> OverlayVSOut {
    let v = overlayVerts[vertexIndex];
    var out : OverlayVSOut;
    let c = v.color;
    let r = f32(c & 255u) / 255.0;
    let g = f32((c >> 8u) & 255u) / 255.0;
    let b = f32((c >> 16u) & 255u) / 255.0;
    let a = f32((c >> 24u) & 255u) / 255.0;
    out.color    = vec4<f32>(r, g, b, a);
    out.uv       = v.uv;
    out.mode     = v.mode;
    out.localPos = v.pad;
    out.halfSize = abs(v.pad);
    out.radius   = v.uv.x;

    let vpW = max(u.params.y, 1.0);
    let vpH = max(u.params.z, 1.0);
    let ndcX = (v.pos.x / vpW) * 2.0 - 1.0;
    let ndcY = 1.0 - (v.pos.y / vpH) * 2.0;
    out.clipPos = vec4<f32>(ndcX, ndcY, 0.0, 1.0);
    return out;
}

@fragment
fn fs_overlay2d(in : OverlayVSOut) -> @location(0) vec4<f32> {
    if (in.mode == 2u) {
        let q = abs(in.localPos) - in.halfSize + vec2<f32>(in.radius, in.radius);
        let dist = length(max(q, vec2<f32>(0.0, 0.0))) + min(max(q.x, q.y), 0.0) - in.radius;
        let alpha = clamp(0.5 - dist, 0.0, 1.0);
        if (alpha <= 0.0) {
            discard;
        }
        return vec4<f32>(in.color.rgb, in.color.a * alpha);
    }
    return in.color;
}
)";

WGPUShaderModule createShaderModule(WGPUDevice device, const char* wgslSource) {
    WGPUShaderSourceWGSL wgslDesc{};
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = WGPUStringView{ wgslSource, std::strlen(wgslSource) };

    WGPUShaderModuleDescriptor smDesc{};
    smDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
    return wgpuDeviceCreateShaderModule(device, &smDesc);
}

void fillSceneUniforms(const ChartWorld& world, uint32_t width, uint32_t height, float timeSec, SceneUniforms& outU) {
    const auto& cam = world.camera();
    AtmosphereConfig atm = world.atmosphere();
    if (world.useDayNightCycle()) {
        atm = AtmosphereConfig::evaluateDayNight(world.timeOfDay());
    }

    float aspect = static_cast<float>(width) / static_cast<float>(std::max(1u, height));
    float fovRad = cam.fovDegrees * (3.1415926535f / 180.0f);

    Mat4 view = Mat4::lookAt(cam.position, cam.target, {0.0f, 1.0f, 0.0f});
    Mat4 proj = Mat4::perspective(fovRad, aspect, 1.0f, 280.0f);
    outU.viewProj = Mat4::multiply(proj, view);

    Vec3f sunDir = atm.sunDirection;
    float sLen = std::sqrt(sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z);
    if (sLen > 1e-6f) { sunDir.x /= sLen; sunDir.y /= sLen; sunDir.z /= sLen; }

    Vec3f sceneCenter = cam.target;
    const float shadowTexelSize = 110.0f / 2048.0f;
    sceneCenter.x = std::round(sceneCenter.x / shadowTexelSize) * shadowTexelSize;
    sceneCenter.z = std::round(sceneCenter.z / shadowTexelSize) * shadowTexelSize;

    Vec3f lightEye{
        sceneCenter.x + sunDir.x * 90.0f,
        sceneCenter.y + sunDir.y * 90.0f,
        sceneCenter.z + sunDir.z * 90.0f
    };
    Mat4 lightView = Mat4::lookAt(lightEye, sceneCenter, {0.0f, 1.0f, 0.0f});
    Mat4 lightProj = Mat4::ortho(-55.0f, 55.0f, -45.0f, 45.0f, 5.0f, 190.0f);
    outU.lightViewProj = Mat4::multiply(lightProj, lightView);

    outU.camPos[0] = cam.position.x;
    outU.camPos[1] = cam.position.y;
    outU.camPos[2] = cam.position.z;
    outU.camPos[3] = timeSec;

    outU.sunDir[0] = sunDir.x;
    outU.sunDir[1] = sunDir.y;
    outU.sunDir[2] = sunDir.z;
    outU.sunDir[3] = atm.fogDensity;

    outU.sunColor[0] = atm.sunColor.x;
    outU.sunColor[1] = atm.sunColor.y;
    outU.sunColor[2] = atm.sunColor.z;
    outU.sunColor[3] = atm.exposure;

    outU.skyZenith[0] = atm.skyZenith.x;
    outU.skyZenith[1] = atm.skyZenith.y;
    outU.skyZenith[2] = atm.skyZenith.z;
    outU.skyZenith[3] = cam.focusDistance;

    outU.skyHorizon[0] = atm.skyHorizon.x;
    outU.skyHorizon[1] = atm.skyHorizon.y;
    outU.skyHorizon[2] = atm.skyHorizon.z;
    outU.skyHorizon[3] = cam.focusRange;

    outU.params[0] = cam.bokehRadius;
    outU.params[1] = static_cast<float>(width);
    outU.params[2] = static_cast<float>(height);
    outU.params[3] = cam.horizonCurve * (1.0f - cam.atlasZoom * 0.85f);

    Vec3f fwd{cam.target.x - cam.position.x, cam.target.y - cam.position.y, cam.target.z - cam.position.z};
    float fL = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (fL > 1e-6f) { fwd.x /= fL; fwd.y /= fL; fwd.z /= fL; }
    Vec3f right{fwd.z, 0.0f, -fwd.x};
    float rL = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rL > 1e-6f) { right.x /= rL; right.y /= rL; right.z /= rL; }
    Vec3f upVec{
        fwd.y * right.z - fwd.z * right.y,
        fwd.z * right.x - fwd.x * right.z,
        fwd.x * right.y - fwd.y * right.x
    };

    outU.camBasisX[0] = right.x; outU.camBasisX[1] = right.y; outU.camBasisX[2] = right.z;
    outU.camBasisX[3] = static_cast<float>(std::min<size_t>(world.telegraphs().size(), 64u));

    outU.camBasisY[0] = upVec.x; outU.camBasisY[1] = upVec.y; outU.camBasisY[2] = upVec.z;
    outU.camBasisY[3] = static_cast<float>(std::min<size_t>(world.pointLights().size(), 64u));

    outU.camBasisZ[0] = fwd.x;   outU.camBasisZ[1] = fwd.y;   outU.camBasisZ[2] = fwd.z;
    outU.camBasisZ[3] = atm.inkDensity;

    outU.heroPos[0] = world.hero().pos.x;
    outU.heroPos[1] = world.hero().pos.y;
    outU.heroPos[2] = world.hero().pos.z;
    outU.heroPos[3] = cam.atlasZoom;
}

struct GpuChartContext {
    WGPUInstance instance{nullptr};
    WGPUSurface surface{nullptr};
    WGPUAdapter adapter{nullptr};
    WGPUDevice device{nullptr};
    WGPUQueue queue{nullptr};
    WGPUTextureFormat outputFormat{WGPUTextureFormat_RGBA8Unorm};

    WGPURenderPipeline shadowWorldPipeline{nullptr};
    WGPURenderPipeline shadowSpritePipeline{nullptr};
    WGPURenderPipeline skyPipeline{nullptr};
    WGPURenderPipeline worldPipeline{nullptr};
    WGPURenderPipeline spritePipeline{nullptr};
    WGPURenderPipeline postPipeline{nullptr};
    WGPURenderPipeline overlayPipeline{nullptr};

    WGPUBindGroupLayout shadowBGL{nullptr};
    WGPUBindGroupLayout sceneBGL{nullptr};
    WGPUBindGroupLayout postBGL{nullptr};
    WGPUBindGroupLayout overlayBGL{nullptr};
    WGPUBindGroup shadowBindGroup{nullptr};
    WGPUBindGroup sceneBindGroup{nullptr};
    WGPUBindGroup postBindGroup{nullptr};
    WGPUBindGroup overlayBindGroup{nullptr};

    WGPUBuffer uniformBuffer{nullptr};
    WGPUBuffer worldVertBuffer{nullptr};
    size_t worldVertBufferCapacity{0};
    WGPUBuffer spriteBuffer{nullptr};
    size_t spriteBufferCapacity{0};
    WGPUBuffer pointLightBuffer{nullptr};
    WGPUBuffer telegraphBuffer{nullptr};
    WGPUBuffer overlayVertBuffer{nullptr};
    size_t overlayVertBufferCapacity{0};

    WGPUTexture albedoAtlasTex{nullptr};
    WGPUTextureView albedoAtlasView{nullptr};
    WGPUTexture normalAtlasTex{nullptr};
    WGPUTextureView normalAtlasView{nullptr};
    WGPUSampler atlasSampler{nullptr};

    WGPUTexture shadowTexture{nullptr};
    WGPUTextureView shadowView{nullptr};
    WGPUSampler shadowSampler{nullptr};

    uint32_t targetWidth{0};
    uint32_t targetHeight{0};
    WGPUTexture depthTexture{nullptr};
    WGPUTextureView depthView{nullptr};
    WGPUTexture hdrColorTexture{nullptr};
    WGPUTextureView hdrColorView{nullptr};
    WGPUSampler linearSampler{nullptr};

    uint32_t uploadedWorldVertexCount{0};
    uint32_t uploadedSpriteCount{0};
    uint32_t uploadedOverlayVertexCount{0};

    bool init(GLFWwindow* optWindow, uint32_t width, uint32_t height, const InkAtlasData& atlas) {
        targetWidth = width;
        targetHeight = height;

        WGPUInstanceDescriptor instDesc{};
        instance = wgpuCreateInstance(&instDesc);
        if (!instance) return false;

        if (optWindow) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
            HWND hwnd = glfwGetWin32Window(optWindow);
            WGPUSurfaceSourceWindowsHWND winDesc{};
            winDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
            winDesc.hinstance = GetModuleHandle(nullptr);
            winDesc.hwnd = hwnd;

            WGPUSurfaceDescriptor surfDesc{};
            surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&winDesc);
            surface = wgpuInstanceCreateSurface(instance, &surfDesc);
#endif
        }

        struct ReqData {
            WGPUAdapter adapter{nullptr};
            WGPUDevice device{nullptr};
        } req;

        WGPURequestAdapterOptions aOpt{};
        aOpt.compatibleSurface = surface;
        aOpt.powerPreference = WGPUPowerPreference_HighPerformance;

        WGPURequestAdapterCallbackInfo aCb{};
        aCb.mode = WGPUCallbackMode_AllowProcessEvents;
        aCb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adp, WGPUStringView, void* u, void*) {
            if (status == WGPURequestAdapterStatus_Success) static_cast<ReqData*>(u)->adapter = adp;
        };
        aCb.userdata1 = &req;
        wgpuInstanceRequestAdapter(instance, &aOpt, aCb);
        wgpuInstanceProcessEvents(instance);
        adapter = req.adapter;
        if (!adapter) return false;

        WGPUDeviceDescriptor dDesc{};
        WGPURequestDeviceCallbackInfo dCb{};
        dCb.mode = WGPUCallbackMode_AllowProcessEvents;
        dCb.callback = [](WGPURequestDeviceStatus status, WGPUDevice dev, WGPUStringView, void* u, void*) {
            if (status == WGPURequestDeviceStatus_Success) static_cast<ReqData*>(u)->device = dev;
        };
        dCb.userdata1 = &req;
        wgpuAdapterRequestDevice(adapter, &dDesc, dCb);
        wgpuInstanceProcessEvents(instance);
        device = req.device;
        if (!device) return false;

        queue = wgpuDeviceGetQueue(device);

        if (surface) {
            WGPUSurfaceCapabilities caps{};
            wgpuSurfaceGetCapabilities(surface, adapter, &caps);
            outputFormat = (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
            configureSurface(width, height);
        } else {
            outputFormat = WGPUTextureFormat_RGBA8Unorm;
        }

        return createPipelinesAndResources(width, height, atlas);
    }

    void configureSurface(uint32_t width, uint32_t height) {
        if (!surface) return;
        WGPUSurfaceConfiguration config{};
        config.device = device;
        config.format = outputFormat;
        config.usage = WGPUTextureUsage_RenderAttachment;
        config.alphaMode = WGPUCompositeAlphaMode_Auto;
        config.width = width;
        config.height = height;
        config.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(surface, &config);
    }

    bool createPipelinesAndResources(uint32_t width, uint32_t height, const InkAtlasData& atlas) {
        std::string sceneWgsl = std::string(kSceneWgslPart1) + kSceneWgslPart2;
        WGPUShaderModule shadowSM  = createShaderModule(device, kShadowWgslSource);
        WGPUShaderModule sceneSM   = createShaderModule(device, sceneWgsl.c_str());
        WGPUShaderModule postSM    = createShaderModule(device, kPostProcessWgslSource);
        WGPUShaderModule overlaySM = createShaderModule(device, kOverlay2dWgslSource);
        if (!shadowSM || !sceneSM || !postSM || !overlaySM) return false;

        // 0. Shadow Bind Group Layout
        WGPUBindGroupLayoutEntry shEntries[4]{};
        shEntries[0].binding = 0;
        shEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        shEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
        shEntries[1].binding = 1;
        shEntries[1].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        shEntries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        shEntries[2].binding = 2;
        shEntries[2].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        shEntries[2].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        shEntries[3].binding = 3;
        shEntries[3].visibility = WGPUShaderStage_Fragment;
        shEntries[3].texture.sampleType = WGPUTextureSampleType_Float;
        shEntries[3].texture.viewDimension = WGPUTextureViewDimension_2DArray;

        WGPUBindGroupLayoutDescriptor shBGLDesc{};
        shBGLDesc.entryCount = 4;
        shBGLDesc.entries = shEntries;
        shadowBGL = wgpuDeviceCreateBindGroupLayout(device, &shBGLDesc);

        WGPUPipelineLayoutDescriptor shPLDesc{};
        shPLDesc.bindGroupLayoutCount = 1;
        shPLDesc.bindGroupLayouts = &shadowBGL;
        WGPUPipelineLayout shadowPL = wgpuDeviceCreatePipelineLayout(device, &shPLDesc);

        WGPUDepthStencilState shadowDepthState{};
        shadowDepthState.format = WGPUTextureFormat_Depth32Float;
        shadowDepthState.depthWriteEnabled = WGPUOptionalBool_True;
        shadowDepthState.depthCompare = WGPUCompareFunction_Less;

        WGPURenderPipelineDescriptor shWorldDesc{};
        shWorldDesc.layout = shadowPL;
        shWorldDesc.vertex.module = shadowSM;
        shWorldDesc.vertex.entryPoint = WGPUStringView{"vs_shadow_world", 15};
        shWorldDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        shWorldDesc.primitive.cullMode = WGPUCullMode_None;
        shWorldDesc.depthStencil = &shadowDepthState;
        shWorldDesc.multisample.count = 1;
        shWorldDesc.multisample.mask = ~0u;
        shadowWorldPipeline = wgpuDeviceCreateRenderPipeline(device, &shWorldDesc);

        WGPUFragmentState shSpriteFrag{};
        shSpriteFrag.module = shadowSM;
        shSpriteFrag.entryPoint = WGPUStringView{"fs_shadow_sprite", 16};
        shSpriteFrag.targetCount = 0;
        shSpriteFrag.targets = nullptr;

        WGPURenderPipelineDescriptor shSpriteDesc{};
        shSpriteDesc.layout = shadowPL;
        shSpriteDesc.vertex.module = shadowSM;
        shSpriteDesc.vertex.entryPoint = WGPUStringView{"vs_shadow_sprite", 16};
        shSpriteDesc.fragment = &shSpriteFrag;
        shSpriteDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        shSpriteDesc.primitive.cullMode = WGPUCullMode_None;
        shSpriteDesc.depthStencil = &shadowDepthState;
        shSpriteDesc.multisample.count = 1;
        shSpriteDesc.multisample.mask = ~0u;
        shadowSpritePipeline = wgpuDeviceCreateRenderPipeline(device, &shSpriteDesc);

        wgpuPipelineLayoutRelease(shadowPL);
        wgpuShaderModuleRelease(shadowSM);

        // 1. Main Scene Bind Group Layout (10 bindings)
        WGPUBindGroupLayoutEntry bglEntries[10]{};
        bglEntries[0].binding = 0;
        bglEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        bglEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
        for (uint32_t b = 1; b <= 4; ++b) {
            bglEntries[b].binding = b;
            bglEntries[b].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
            bglEntries[b].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        }
        bglEntries[5].binding = 5;
        bglEntries[5].visibility = WGPUShaderStage_Fragment;
        bglEntries[5].texture.sampleType = WGPUTextureSampleType_Float;
        bglEntries[5].texture.viewDimension = WGPUTextureViewDimension_2DArray;
        bglEntries[6].binding = 6;
        bglEntries[6].visibility = WGPUShaderStage_Fragment;
        bglEntries[6].texture.sampleType = WGPUTextureSampleType_Float;
        bglEntries[6].texture.viewDimension = WGPUTextureViewDimension_2DArray;
        bglEntries[7].binding = 7;
        bglEntries[7].visibility = WGPUShaderStage_Fragment;
        bglEntries[7].sampler.type = WGPUSamplerBindingType_Filtering;
        bglEntries[8].binding = 8;
        bglEntries[8].visibility = WGPUShaderStage_Fragment;
        bglEntries[8].texture.sampleType = WGPUTextureSampleType_Depth;
        bglEntries[8].texture.viewDimension = WGPUTextureViewDimension_2D;
        bglEntries[9].binding = 9;
        bglEntries[9].visibility = WGPUShaderStage_Fragment;
        bglEntries[9].sampler.type = WGPUSamplerBindingType_Comparison;

        WGPUBindGroupLayoutDescriptor bglDesc{};
        bglDesc.entryCount = 10;
        bglDesc.entries = bglEntries;
        sceneBGL = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

        WGPUPipelineLayoutDescriptor plDesc{};
        plDesc.bindGroupLayoutCount = 1;
        plDesc.bindGroupLayouts = &sceneBGL;
        WGPUPipelineLayout scenePL = wgpuDeviceCreatePipelineLayout(device, &plDesc);

        WGPUColorTargetState hdrTarget{};
        hdrTarget.format = WGPUTextureFormat_RGBA16Float;
        hdrTarget.writeMask = WGPUColorWriteMask_All;

        WGPUDepthStencilState skyDepth{};
        skyDepth.format = WGPUTextureFormat_Depth32Float;
        skyDepth.depthWriteEnabled = WGPUOptionalBool_False;
        skyDepth.depthCompare = WGPUCompareFunction_Always;

        WGPUFragmentState skyFrag{};
        skyFrag.module = sceneSM;
        skyFrag.entryPoint = WGPUStringView{"fs_sky", 6};
        skyFrag.targetCount = 1;
        skyFrag.targets = &hdrTarget;

        WGPURenderPipelineDescriptor skyDesc{};
        skyDesc.layout = scenePL;
        skyDesc.vertex.module = sceneSM;
        skyDesc.vertex.entryPoint = WGPUStringView{"vs_sky", 6};
        skyDesc.fragment = &skyFrag;
        skyDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        skyDesc.depthStencil = &skyDepth;
        skyDesc.multisample.count = 1;
        skyDesc.multisample.mask = ~0u;
        skyPipeline = wgpuDeviceCreateRenderPipeline(device, &skyDesc);

        WGPUDepthStencilState geoDepth{};
        geoDepth.format = WGPUTextureFormat_Depth32Float;
        geoDepth.depthWriteEnabled = WGPUOptionalBool_True;
        geoDepth.depthCompare = WGPUCompareFunction_LessEqual;

        WGPUFragmentState worldFrag{};
        worldFrag.module = sceneSM;
        worldFrag.entryPoint = WGPUStringView{"fs_world", 8};
        worldFrag.targetCount = 1;
        worldFrag.targets = &hdrTarget;

        WGPURenderPipelineDescriptor worldDesc{};
        worldDesc.layout = scenePL;
        worldDesc.vertex.module = sceneSM;
        worldDesc.vertex.entryPoint = WGPUStringView{"vs_world", 8};
        worldDesc.fragment = &worldFrag;
        worldDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        worldDesc.primitive.cullMode = WGPUCullMode_None;
        worldDesc.depthStencil = &geoDepth;
        worldDesc.multisample.count = 1;
        worldDesc.multisample.mask = ~0u;
        worldPipeline = wgpuDeviceCreateRenderPipeline(device, &worldDesc);

        WGPUFragmentState spriteFrag{};
        spriteFrag.module = sceneSM;
        spriteFrag.entryPoint = WGPUStringView{"fs_sprite", 9};
        spriteFrag.targetCount = 1;
        spriteFrag.targets = &hdrTarget;

        WGPURenderPipelineDescriptor spriteDesc{};
        spriteDesc.layout = scenePL;
        spriteDesc.vertex.module = sceneSM;
        spriteDesc.vertex.entryPoint = WGPUStringView{"vs_sprite", 9};
        spriteDesc.fragment = &spriteFrag;
        spriteDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        spriteDesc.primitive.cullMode = WGPUCullMode_None;
        spriteDesc.depthStencil = &geoDepth;
        spriteDesc.multisample.count = 1;
        spriteDesc.multisample.mask = ~0u;
        spritePipeline = wgpuDeviceCreateRenderPipeline(device, &spriteDesc);

        wgpuPipelineLayoutRelease(scenePL);
        wgpuShaderModuleRelease(sceneSM);

        // 2. Post-Process Pipeline
        WGPUBindGroupLayoutEntry postEntries[3]{};
        postEntries[0].binding = 0;
        postEntries[0].visibility = WGPUShaderStage_Fragment;
        postEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
        postEntries[1].binding = 1;
        postEntries[1].visibility = WGPUShaderStage_Fragment;
        postEntries[1].texture.sampleType = WGPUTextureSampleType_Float;
        postEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
        postEntries[2].binding = 2;
        postEntries[2].visibility = WGPUShaderStage_Fragment;
        postEntries[2].sampler.type = WGPUSamplerBindingType_Filtering;

        WGPUBindGroupLayoutDescriptor postBGLDesc{};
        postBGLDesc.entryCount = 3;
        postBGLDesc.entries = postEntries;
        postBGL = wgpuDeviceCreateBindGroupLayout(device, &postBGLDesc);

        WGPUPipelineLayoutDescriptor postPLDesc{};
        postPLDesc.bindGroupLayoutCount = 1;
        postPLDesc.bindGroupLayouts = &postBGL;
        WGPUPipelineLayout postPL = wgpuDeviceCreatePipelineLayout(device, &postPLDesc);

        WGPUColorTargetState outTarget{};
        outTarget.format = outputFormat;
        outTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState postFrag{};
        postFrag.module = postSM;
        postFrag.entryPoint = WGPUStringView{"fs_post", 7};
        postFrag.targetCount = 1;
        postFrag.targets = &outTarget;

        WGPURenderPipelineDescriptor postDesc{};
        postDesc.layout = postPL;
        postDesc.vertex.module = postSM;
        postDesc.vertex.entryPoint = WGPUStringView{"vs_post", 7};
        postDesc.fragment = &postFrag;
        postDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        postDesc.multisample.count = 1;
        postDesc.multisample.mask = ~0u;
        postPipeline = wgpuDeviceCreateRenderPipeline(device, &postDesc);

        wgpuPipelineLayoutRelease(postPL);
        wgpuShaderModuleRelease(postSM);

        // 3. 2D Overlay Pipeline
        WGPUBindGroupLayoutEntry ovEntries[2]{};
        ovEntries[0].binding = 0;
        ovEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        ovEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
        ovEntries[1].binding = 1;
        ovEntries[1].visibility = WGPUShaderStage_Vertex;
        ovEntries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

        WGPUBindGroupLayoutDescriptor ovBGLDesc{};
        ovBGLDesc.entryCount = 2;
        ovBGLDesc.entries = ovEntries;
        overlayBGL = wgpuDeviceCreateBindGroupLayout(device, &ovBGLDesc);

        WGPUPipelineLayoutDescriptor ovPLDesc{};
        ovPLDesc.bindGroupLayoutCount = 1;
        ovPLDesc.bindGroupLayouts = &overlayBGL;
        WGPUPipelineLayout ovPL = wgpuDeviceCreatePipelineLayout(device, &ovPLDesc);

        WGPUBlendState ovBlend{};
        ovBlend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
        ovBlend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        ovBlend.color.operation = WGPUBlendOperation_Add;
        ovBlend.alpha.srcFactor = WGPUBlendFactor_One;
        ovBlend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        ovBlend.alpha.operation = WGPUBlendOperation_Add;

        WGPUColorTargetState ovTarget{};
        ovTarget.format = outputFormat;
        ovTarget.blend = &ovBlend;
        ovTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState ovFrag{};
        ovFrag.module = overlaySM;
        ovFrag.entryPoint = WGPUStringView{"fs_overlay2d", 12};
        ovFrag.targetCount = 1;
        ovFrag.targets = &ovTarget;

        WGPURenderPipelineDescriptor ovDesc{};
        ovDesc.layout = ovPL;
        ovDesc.vertex.module = overlaySM;
        ovDesc.vertex.entryPoint = WGPUStringView{"vs_overlay2d", 12};
        ovDesc.fragment = &ovFrag;
        ovDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        ovDesc.primitive.cullMode = WGPUCullMode_None;
        ovDesc.multisample.count = 1;
        ovDesc.multisample.mask = ~0u;
        overlayPipeline = wgpuDeviceCreateRenderPipeline(device, &ovDesc);

        wgpuPipelineLayoutRelease(ovPL);
        wgpuShaderModuleRelease(overlaySM);

        // 4. Create Uniform, PointLight & Telegraph Buffers + Texture2DArray Atlases
        WGPUBufferDescriptor uDesc{};
        uDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        uDesc.size = sizeof(SceneUniforms);
        uniformBuffer = wgpuDeviceCreateBuffer(device, &uDesc);

        WGPUBufferDescriptor plBufDesc{};
        plBufDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
        plBufDesc.size = sizeof(PointLight) * 64;
        pointLightBuffer = wgpuDeviceCreateBuffer(device, &plBufDesc);

        WGPUBufferDescriptor tgBufDesc{};
        tgBufDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
        tgBufDesc.size = sizeof(CombatTelegraph) * 64;
        telegraphBuffer = wgpuDeviceCreateBuffer(device, &tgBufDesc);

        WGPUTextureDescriptor atlasDesc{};
        atlasDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        atlasDesc.dimension = WGPUTextureDimension_2D;
        atlasDesc.size = WGPUExtent3D{atlas.width, atlas.height, atlas.layers};
        atlasDesc.format = WGPUTextureFormat_RGBA8Unorm;
        atlasDesc.mipLevelCount = 1;
        atlasDesc.sampleCount = 1;

        albedoAtlasTex = wgpuDeviceCreateTexture(device, &atlasDesc);
        normalAtlasTex = wgpuDeviceCreateTexture(device, &atlasDesc);

        WGPUTextureViewDescriptor arrViewDesc{};
        arrViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
        arrViewDesc.dimension = WGPUTextureViewDimension_2DArray;
        arrViewDesc.baseMipLevel = 0;
        arrViewDesc.mipLevelCount = 1;
        arrViewDesc.baseArrayLayer = 0;
        arrViewDesc.arrayLayerCount = atlas.layers;

        albedoAtlasView = wgpuTextureCreateView(albedoAtlasTex, &arrViewDesc);
        normalAtlasView = wgpuTextureCreateView(normalAtlasTex, &arrViewDesc);

        WGPUTexelCopyTextureInfo dstTex{};
        dstTex.texture = albedoAtlasTex;
        dstTex.mipLevel = 0;
        dstTex.origin = WGPUOrigin3D{0, 0, 0};
        dstTex.aspect = WGPUTextureAspect_All;

        WGPUTexelCopyBufferLayout texLayout{};
        texLayout.offset = 0;
        texLayout.bytesPerRow = atlas.width * 4;
        texLayout.rowsPerImage = atlas.height;

        WGPUExtent3D copySize{atlas.width, atlas.height, atlas.layers};
        wgpuQueueWriteTexture(queue, &dstTex, atlas.albedoBytes.data(), atlas.albedoBytes.size(), &texLayout, &copySize);
        dstTex.texture = normalAtlasTex;
        wgpuQueueWriteTexture(queue, &dstTex, atlas.normalBytes.data(), atlas.normalBytes.size(), &texLayout, &copySize);

        WGPUSamplerDescriptor sampDesc{};
        sampDesc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampDesc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampDesc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampDesc.magFilter = WGPUFilterMode_Linear;
        sampDesc.minFilter = WGPUFilterMode_Linear;
        sampDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
        sampDesc.maxAnisotropy = 1;
        atlasSampler  = wgpuDeviceCreateSampler(device, &sampDesc);
        linearSampler = wgpuDeviceCreateSampler(device, &sampDesc);

        WGPUTextureDescriptor shTexDesc{};
        shTexDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
        shTexDesc.dimension = WGPUTextureDimension_2D;
        shTexDesc.size = WGPUExtent3D{2048, 2048, 1};
        shTexDesc.format = WGPUTextureFormat_Depth32Float;
        shTexDesc.mipLevelCount = 1;
        shTexDesc.sampleCount = 1;
        shadowTexture = wgpuDeviceCreateTexture(device, &shTexDesc);
        shadowView    = wgpuTextureCreateView(shadowTexture, nullptr);

        WGPUSamplerDescriptor shSampDesc{};
        shSampDesc.addressModeU = WGPUAddressMode_ClampToEdge;
        shSampDesc.addressModeV = WGPUAddressMode_ClampToEdge;
        shSampDesc.addressModeW = WGPUAddressMode_ClampToEdge;
        shSampDesc.magFilter = WGPUFilterMode_Linear;
        shSampDesc.minFilter = WGPUFilterMode_Linear;
        shSampDesc.compare = WGPUCompareFunction_LessEqual;
        shSampDesc.maxAnisotropy = 1;
        shadowSampler = wgpuDeviceCreateSampler(device, &shSampDesc);

        reallocateViewportTargets(width, height);
        return true;
    }

    void reallocateViewportTargets(uint32_t width, uint32_t height) {
        targetWidth = width;
        targetHeight = height;

        if (depthView) { wgpuTextureViewRelease(depthView); depthView = nullptr; }
        if (depthTexture) { wgpuTextureDestroy(depthTexture); wgpuTextureRelease(depthTexture); depthTexture = nullptr; }
        if (hdrColorView) { wgpuTextureViewRelease(hdrColorView); hdrColorView = nullptr; }
        if (hdrColorTexture) { wgpuTextureDestroy(hdrColorTexture); wgpuTextureRelease(hdrColorTexture); hdrColorTexture = nullptr; }
        if (postBindGroup) { wgpuBindGroupRelease(postBindGroup); postBindGroup = nullptr; }

        WGPUTextureDescriptor dDesc{};
        dDesc.usage = WGPUTextureUsage_RenderAttachment;
        dDesc.dimension = WGPUTextureDimension_2D;
        dDesc.size = WGPUExtent3D{width, height, 1};
        dDesc.format = WGPUTextureFormat_Depth32Float;
        dDesc.mipLevelCount = 1;
        dDesc.sampleCount = 1;
        depthTexture = wgpuDeviceCreateTexture(device, &dDesc);
        depthView    = wgpuTextureCreateView(depthTexture, nullptr);

        WGPUTextureDescriptor hdrDesc{};
        hdrDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
        hdrDesc.dimension = WGPUTextureDimension_2D;
        hdrDesc.size = WGPUExtent3D{width, height, 1};
        hdrDesc.format = WGPUTextureFormat_RGBA16Float;
        hdrDesc.mipLevelCount = 1;
        hdrDesc.sampleCount = 1;
        hdrColorTexture = wgpuDeviceCreateTexture(device, &hdrDesc);
        hdrColorView    = wgpuTextureCreateView(hdrColorTexture, nullptr);

        WGPUBindGroupEntry postEntries[3]{};
        postEntries[0].binding = 0;
        postEntries[0].buffer = uniformBuffer;
        postEntries[0].size = sizeof(SceneUniforms);
        postEntries[1].binding = 1;
        postEntries[1].textureView = hdrColorView;
        postEntries[2].binding = 2;
        postEntries[2].sampler = linearSampler;

        WGPUBindGroupDescriptor postBgDesc{};
        postBgDesc.layout = postBGL;
        postBgDesc.entryCount = 3;
        postBgDesc.entries = postEntries;
        postBindGroup = wgpuDeviceCreateBindGroup(device, &postBgDesc);
    }

    void uploadWorldAndDynamicEntities(const ChartWorld& world, bool rebuildStaticWorldVerts = true) {
        bool needRebind = false;

        if (rebuildStaticWorldVerts || !worldVertBuffer) {
            const auto& wVerts = world.worldVertices();
            uploadedWorldVertexCount = static_cast<uint32_t>(wVerts.size());
            size_t wBytes = std::max<size_t>(sizeof(ChartVertex) * 6, wVerts.size() * sizeof(ChartVertex));

            if (!worldVertBuffer || wBytes > worldVertBufferCapacity) {
                if (worldVertBuffer) {
                    wgpuBufferDestroy(worldVertBuffer);
                    wgpuBufferRelease(worldVertBuffer);
                }
                worldVertBufferCapacity = std::max(wBytes * 5 / 4, static_cast<size_t>(16384 * sizeof(ChartVertex)));
                WGPUBufferDescriptor bDesc{};
                bDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
                bDesc.size = worldVertBufferCapacity;
                worldVertBuffer = wgpuDeviceCreateBuffer(device, &bDesc);
                needRebind = true;
            }
            if (!wVerts.empty()) {
                wgpuQueueWriteBuffer(queue, worldVertBuffer, 0, wVerts.data(), wVerts.size() * sizeof(ChartVertex));
            }
        }

        const auto& spList = world.standees();
        uploadedSpriteCount = static_cast<uint32_t>(spList.size());
        size_t spBytes = std::max<size_t>(sizeof(StandeeInstance) * 4, spList.size() * sizeof(StandeeInstance));
        if (!spriteBuffer || spBytes > spriteBufferCapacity) {
            if (spriteBuffer) {
                wgpuBufferDestroy(spriteBuffer);
                wgpuBufferRelease(spriteBuffer);
            }
            spriteBufferCapacity = std::max(spBytes * 3 / 2, static_cast<size_t>(2048 * sizeof(StandeeInstance)));
            WGPUBufferDescriptor sDesc{};
            sDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
            sDesc.size = spriteBufferCapacity;
            spriteBuffer = wgpuDeviceCreateBuffer(device, &sDesc);
            needRebind = true;
        }
        if (!spList.empty()) {
            wgpuQueueWriteBuffer(queue, spriteBuffer, 0, spList.data(), spList.size() * sizeof(StandeeInstance));
        }

        const auto& lights = world.pointLights();
        if (!lights.empty()) {
            size_t count = std::min<size_t>(lights.size(), 64u);
            wgpuQueueWriteBuffer(queue, pointLightBuffer, 0, lights.data(), count * sizeof(PointLight));
        }

        const auto& tgs = world.telegraphs();
        if (!tgs.empty()) {
            size_t count = std::min<size_t>(tgs.size(), 64u);
            wgpuQueueWriteBuffer(queue, telegraphBuffer, 0, tgs.data(), count * sizeof(CombatTelegraph));
        }

        if (needRebind || !shadowBindGroup || !sceneBindGroup) {
            if (shadowBindGroup) wgpuBindGroupRelease(shadowBindGroup);
            if (sceneBindGroup)  wgpuBindGroupRelease(sceneBindGroup);

            WGPUBindGroupEntry shEntries[4]{};
            shEntries[0].binding = 0; shEntries[0].buffer = uniformBuffer;   shEntries[0].size = sizeof(SceneUniforms);
            shEntries[1].binding = 1; shEntries[1].buffer = worldVertBuffer; shEntries[1].size = worldVertBufferCapacity;
            shEntries[2].binding = 2; shEntries[2].buffer = spriteBuffer;    shEntries[2].size = spriteBufferCapacity;
            shEntries[3].binding = 3; shEntries[3].textureView = albedoAtlasView;

            WGPUBindGroupDescriptor shBgDesc{};
            shBgDesc.layout = shadowBGL;
            shBgDesc.entryCount = 4;
            shBgDesc.entries = shEntries;
            shadowBindGroup = wgpuDeviceCreateBindGroup(device, &shBgDesc);

            WGPUBindGroupEntry scEntries[10]{};
            scEntries[0].binding = 0; scEntries[0].buffer = uniformBuffer;    scEntries[0].size = sizeof(SceneUniforms);
            scEntries[1].binding = 1; scEntries[1].buffer = worldVertBuffer;  scEntries[1].size = worldVertBufferCapacity;
            scEntries[2].binding = 2; scEntries[2].buffer = spriteBuffer;     scEntries[2].size = spriteBufferCapacity;
            scEntries[3].binding = 3; scEntries[3].buffer = pointLightBuffer; scEntries[3].size = sizeof(PointLight) * 64;
            scEntries[4].binding = 4; scEntries[4].buffer = telegraphBuffer;  scEntries[4].size = sizeof(CombatTelegraph) * 64;
            scEntries[5].binding = 5; scEntries[5].textureView = albedoAtlasView;
            scEntries[6].binding = 6; scEntries[6].textureView = normalAtlasView;
            scEntries[7].binding = 7; scEntries[7].sampler = atlasSampler;
            scEntries[8].binding = 8; scEntries[8].textureView = shadowView;
            scEntries[9].binding = 9; scEntries[9].sampler = shadowSampler;

            WGPUBindGroupDescriptor scBgDesc{};
            scBgDesc.layout = sceneBGL;
            scBgDesc.entryCount = 10;
            scBgDesc.entries = scEntries;
            sceneBindGroup = wgpuDeviceCreateBindGroup(device, &scBgDesc);
        }

        uploadOverlay2D(world);
    }

    void uploadOverlay2D(const ChartWorld& world) {
        const auto& ovVerts = world.overlay2D().getVertices();
        uploadedOverlayVertexCount = static_cast<uint32_t>(ovVerts.size());

        size_t ovBytes = std::max<size_t>(sizeof(eatsbits::ui::Vertex2D) * 64, ovVerts.size() * sizeof(eatsbits::ui::Vertex2D));
        bool needRebind = false;
        if (!overlayVertBuffer || ovBytes > overlayVertBufferCapacity) {
            if (overlayVertBuffer) {
                wgpuBufferDestroy(overlayVertBuffer);
                wgpuBufferRelease(overlayVertBuffer);
            }
            overlayVertBufferCapacity = std::max(ovBytes * 3 / 2, static_cast<size_t>(4096 * sizeof(eatsbits::ui::Vertex2D)));
            WGPUBufferDescriptor oDesc{};
            oDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
            oDesc.size = overlayVertBufferCapacity;
            overlayVertBuffer = wgpuDeviceCreateBuffer(device, &oDesc);
            needRebind = true;
        }

        if (!ovVerts.empty()) {
            wgpuQueueWriteBuffer(queue, overlayVertBuffer, 0, ovVerts.data(), ovVerts.size() * sizeof(eatsbits::ui::Vertex2D));
        }

        if ((needRebind || !overlayBindGroup) && overlayBGL) {
            if (overlayBindGroup) wgpuBindGroupRelease(overlayBindGroup);
            WGPUBindGroupEntry ovEntries[2]{};
            ovEntries[0].binding = 0; ovEntries[0].buffer = uniformBuffer;     ovEntries[0].size = sizeof(SceneUniforms);
            ovEntries[1].binding = 1; ovEntries[1].buffer = overlayVertBuffer; ovEntries[1].size = overlayVertBufferCapacity;

            WGPUBindGroupDescriptor ovBgDesc{};
            ovBgDesc.layout = overlayBGL;
            ovBgDesc.entryCount = 2;
            ovBgDesc.entries = ovEntries;
            overlayBindGroup = wgpuDeviceCreateBindGroup(device, &ovBgDesc);
        }
    }

    void recordFramePasses(WGPUCommandEncoder encoder, WGPUTextureView finalColorTargetView) {
        // Pass 1: 2048x2048 Directional Sun Shadow Map
        WGPURenderPassDepthStencilAttachment shDepthAtt{};
        shDepthAtt.view = shadowView;
        shDepthAtt.depthLoadOp = WGPULoadOp_Clear;
        shDepthAtt.depthStoreOp = WGPUStoreOp_Store;
        shDepthAtt.depthClearValue = 1.0f;

        WGPURenderPassDescriptor shPassDesc{};
        shPassDesc.colorAttachmentCount = 0;
        shPassDesc.depthStencilAttachment = &shDepthAtt;

        WGPURenderPassEncoder shPass = wgpuCommandEncoderBeginRenderPass(encoder, &shPassDesc);
        wgpuRenderPassEncoderSetBindGroup(shPass, 0, shadowBindGroup, 0, nullptr);
        if (uploadedWorldVertexCount > 0) {
            wgpuRenderPassEncoderSetPipeline(shPass, shadowWorldPipeline);
            wgpuRenderPassEncoderDraw(shPass, uploadedWorldVertexCount, 1u, 0u, 0u);
        }
        if (uploadedSpriteCount > 0) {
            wgpuRenderPassEncoderSetPipeline(shPass, shadowSpritePipeline);
            wgpuRenderPassEncoderDraw(shPass, uploadedSpriteCount * 6u, 1u, 0u, 0u);
        }
        wgpuRenderPassEncoderEnd(shPass);
        wgpuRenderPassEncoderRelease(shPass);

        // Pass 2: Living Parchment Cartography & 2.5D Standees
        WGPURenderPassColorAttachment sceneColAtt{};
        sceneColAtt.view = hdrColorView;
        sceneColAtt.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        sceneColAtt.loadOp = WGPULoadOp_Clear;
        sceneColAtt.storeOp = WGPUStoreOp_Store;
        sceneColAtt.clearValue = WGPUColor{0.82, 0.76, 0.64, 240.0};

        WGPURenderPassDepthStencilAttachment sceneDepthAtt{};
        sceneDepthAtt.view = depthView;
        sceneDepthAtt.depthLoadOp = WGPULoadOp_Clear;
        sceneDepthAtt.depthStoreOp = WGPUStoreOp_Store;
        sceneDepthAtt.depthClearValue = 1.0f;

        WGPURenderPassDescriptor scenePassDesc{};
        scenePassDesc.colorAttachmentCount = 1;
        scenePassDesc.colorAttachments = &sceneColAtt;
        scenePassDesc.depthStencilAttachment = &sceneDepthAtt;

        WGPURenderPassEncoder scenePass = wgpuCommandEncoderBeginRenderPass(encoder, &scenePassDesc);
        wgpuRenderPassEncoderSetBindGroup(scenePass, 0, sceneBindGroup, 0, nullptr);

        wgpuRenderPassEncoderSetPipeline(scenePass, skyPipeline);
        wgpuRenderPassEncoderDraw(scenePass, 3u, 1u, 0u, 0u);

        if (uploadedWorldVertexCount > 0) {
            wgpuRenderPassEncoderSetPipeline(scenePass, worldPipeline);
            wgpuRenderPassEncoderDraw(scenePass, uploadedWorldVertexCount, 1u, 0u, 0u);
        }
        if (uploadedSpriteCount > 0) {
            wgpuRenderPassEncoderSetPipeline(scenePass, spritePipeline);
            wgpuRenderPassEncoderDraw(scenePass, uploadedSpriteCount * 6u, 1u, 0u, 0u);
        }
        wgpuRenderPassEncoderEnd(scenePass);
        wgpuRenderPassEncoderRelease(scenePass);

        // Pass 3: Tabletop Tilt-Shift Bokeh & Antique Vellum Grade
        WGPURenderPassColorAttachment postColAtt{};
        postColAtt.view = finalColorTargetView;
        postColAtt.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        postColAtt.loadOp = WGPULoadOp_Clear;
        postColAtt.storeOp = WGPUStoreOp_Store;
        postColAtt.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};

        WGPURenderPassDescriptor postPassDesc{};
        postPassDesc.colorAttachmentCount = 1;
        postPassDesc.colorAttachments = &postColAtt;

        WGPURenderPassEncoder postPass = wgpuCommandEncoderBeginRenderPass(encoder, &postPassDesc);
        wgpuRenderPassEncoderSetPipeline(postPass, postPipeline);
        wgpuRenderPassEncoderSetBindGroup(postPass, 0, postBindGroup, 0, nullptr);
        wgpuRenderPassEncoderDraw(postPass, 3u, 1u, 0u, 0u);
        wgpuRenderPassEncoderEnd(postPass);
        wgpuRenderPassEncoderRelease(postPass);

        // Pass 4: 2D Vector HUD & Typography Overlay
        if (uploadedOverlayVertexCount > 0 && overlayPipeline && overlayBindGroup) {
            WGPURenderPassColorAttachment ovColAtt{};
            ovColAtt.view = finalColorTargetView;
            ovColAtt.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
            ovColAtt.loadOp = WGPULoadOp_Load;
            ovColAtt.storeOp = WGPUStoreOp_Store;

            WGPURenderPassDescriptor ovPassDesc{};
            ovPassDesc.colorAttachmentCount = 1;
            ovPassDesc.colorAttachments = &ovColAtt;

            WGPURenderPassEncoder ovPass = wgpuCommandEncoderBeginRenderPass(encoder, &ovPassDesc);
            wgpuRenderPassEncoderSetPipeline(ovPass, overlayPipeline);
            wgpuRenderPassEncoderSetBindGroup(ovPass, 0, overlayBindGroup, 0, nullptr);
            wgpuRenderPassEncoderDraw(ovPass, uploadedOverlayVertexCount, 1u, 0u, 0u);
            wgpuRenderPassEncoderEnd(ovPass);
            wgpuRenderPassEncoderRelease(ovPass);
        }
    }

    void shutdown() {
        if (shadowBindGroup) wgpuBindGroupRelease(shadowBindGroup);
        if (sceneBindGroup) wgpuBindGroupRelease(sceneBindGroup);
        if (postBindGroup) wgpuBindGroupRelease(postBindGroup);
        if (overlayBindGroup) wgpuBindGroupRelease(overlayBindGroup);
        if (shadowBGL) wgpuBindGroupLayoutRelease(shadowBGL);
        if (sceneBGL) wgpuBindGroupLayoutRelease(sceneBGL);
        if (postBGL) wgpuBindGroupLayoutRelease(postBGL);
        if (overlayBGL) wgpuBindGroupLayoutRelease(overlayBGL);
        if (shadowWorldPipeline) wgpuRenderPipelineRelease(shadowWorldPipeline);
        if (shadowSpritePipeline) wgpuRenderPipelineRelease(shadowSpritePipeline);
        if (skyPipeline) wgpuRenderPipelineRelease(skyPipeline);
        if (worldPipeline) wgpuRenderPipelineRelease(worldPipeline);
        if (spritePipeline) wgpuRenderPipelineRelease(spritePipeline);
        if (postPipeline) wgpuRenderPipelineRelease(postPipeline);
        if (overlayPipeline) wgpuRenderPipelineRelease(overlayPipeline);
        if (uniformBuffer) { wgpuBufferDestroy(uniformBuffer); wgpuBufferRelease(uniformBuffer); }
        if (worldVertBuffer) { wgpuBufferDestroy(worldVertBuffer); wgpuBufferRelease(worldVertBuffer); }
        if (spriteBuffer) { wgpuBufferDestroy(spriteBuffer); wgpuBufferRelease(spriteBuffer); }
        if (pointLightBuffer) { wgpuBufferDestroy(pointLightBuffer); wgpuBufferRelease(pointLightBuffer); }
        if (telegraphBuffer) { wgpuBufferDestroy(telegraphBuffer); wgpuBufferRelease(telegraphBuffer); }
        if (overlayVertBuffer) { wgpuBufferDestroy(overlayVertBuffer); wgpuBufferRelease(overlayVertBuffer); }
        if (albedoAtlasView) wgpuTextureViewRelease(albedoAtlasView);
        if (albedoAtlasTex) { wgpuTextureDestroy(albedoAtlasTex); wgpuTextureRelease(albedoAtlasTex); }
        if (normalAtlasView) wgpuTextureViewRelease(normalAtlasView);
        if (normalAtlasTex) { wgpuTextureDestroy(normalAtlasTex); wgpuTextureRelease(normalAtlasTex); }
        if (shadowView) wgpuTextureViewRelease(shadowView);
        if (shadowTexture) { wgpuTextureDestroy(shadowTexture); wgpuTextureRelease(shadowTexture); }
        if (depthView) wgpuTextureViewRelease(depthView);
        if (depthTexture) { wgpuTextureDestroy(depthTexture); wgpuTextureRelease(depthTexture); }
        if (hdrColorView) wgpuTextureViewRelease(hdrColorView);
        if (hdrColorTexture) { wgpuTextureDestroy(hdrColorTexture); wgpuTextureRelease(hdrColorTexture); }
        if (atlasSampler) wgpuSamplerRelease(atlasSampler);
        if (shadowSampler) wgpuSamplerRelease(shadowSampler);
        if (linearSampler) wgpuSamplerRelease(linearSampler);
        if (surface) wgpuSurfaceRelease(surface);
        if (queue) wgpuQueueRelease(queue);
        if (device) { wgpuDeviceDestroy(device); wgpuDeviceRelease(device); }
        if (adapter) wgpuAdapterRelease(adapter);
        if (instance) wgpuInstanceRelease(instance);
    }
};

uint32_t computeCrc32(const uint8_t* data, size_t len) noexcept {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1u) ^ (0xEDB88320u & (-(static_cast<int32_t>(crc & 1u))));
        }
    }
    return ~crc;
}

void appendBE32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24u) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((val >> 16u) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((val >> 8u) & 0xFFu));
    buf.push_back(static_cast<uint8_t>(val & 0xFFu));
}

void appendPngChunk(std::vector<uint8_t>& fileBytes, const char type[4], const uint8_t* data, size_t len) {
    appendBE32(fileBytes, static_cast<uint32_t>(len));
    size_t typeOffset = fileBytes.size();
    fileBytes.push_back(static_cast<uint8_t>(type[0]));
    fileBytes.push_back(static_cast<uint8_t>(type[1]));
    fileBytes.push_back(static_cast<uint8_t>(type[2]));
    fileBytes.push_back(static_cast<uint8_t>(type[3]));
    if (len > 0 && data) {
        fileBytes.insert(fileBytes.end(), data, data + len);
    }
    uint32_t crc = computeCrc32(fileBytes.data() + typeOffset, 4 + len);
    appendBE32(fileBytes, crc);
}

} // namespace

bool ChartRenderer::writePngFile(const std::string& filePath, uint32_t width, uint32_t height, const uint8_t* rgbaPixels) {
    if (!rgbaPixels || width == 0 || height == 0) return false;

    const size_t rowBytes = static_cast<size_t>(width) * 4 + 1;
    std::vector<uint8_t> rawData(rowBytes * height);
    for (uint32_t y = 0; y < height; ++y) {
        rawData[y * rowBytes] = 0;
        std::memcpy(&rawData[y * rowBytes + 1], &rgbaPixels[static_cast<size_t>(y) * width * 4], static_cast<size_t>(width) * 4);
    }

    std::vector<uint8_t> zlibStream = {0x78, 0x01};
    size_t offset = 0;
    size_t remaining = rawData.size();
    while (remaining > 0) {
        uint16_t blockLen = static_cast<uint16_t>(std::min<size_t>(remaining, 65535u));
        bool isFinal = (remaining - blockLen == 0);
        zlibStream.push_back(isFinal ? 0x01 : 0x00);
        zlibStream.push_back(static_cast<uint8_t>(blockLen & 0xFFu));
        zlibStream.push_back(static_cast<uint8_t>((blockLen >> 8u) & 0xFFu));
        uint16_t nlen = ~blockLen;
        zlibStream.push_back(static_cast<uint8_t>(nlen & 0xFFu));
        zlibStream.push_back(static_cast<uint8_t>((nlen >> 8u) & 0xFFu));
        zlibStream.insert(zlibStream.end(), rawData.data() + offset, rawData.data() + offset + blockLen);
        offset += blockLen;
        remaining -= blockLen;
    }

    uint32_t s1 = 1, s2 = 0;
    for (uint8_t b : rawData) {
        s1 = (s1 + b) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    appendBE32(zlibStream, (s2 << 16u) | s1);

    std::vector<uint8_t> pngBytes = {137, 80, 78, 71, 13, 10, 26, 10};
    std::vector<uint8_t> ihdr;
    appendBE32(ihdr, width);
    appendBE32(ihdr, height);
    ihdr.push_back(8);
    ihdr.push_back(6);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    appendPngChunk(pngBytes, "IHDR", ihdr.data(), ihdr.size());
    appendPngChunk(pngBytes, "IDAT", zlibStream.data(), zlibStream.size());
    appendPngChunk(pngBytes, "IEND", nullptr, 0);

    std::ofstream ofs(filePath, std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs.write(reinterpret_cast<const char*>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    return ofs.good();
}

bool ChartRenderer::renderToPng(ChartWorld& world, uint32_t width, uint32_t height, const std::string& outputPngPath) {
    world.updateSimulation(0.016f, width, height);

    GpuChartContext ctx;
    if (!ctx.init(nullptr, width, height, world.inkAtlas())) {
        std::cerr << "[ChartRenderer] Failed to initialize headless WebGPU device.\n";
        return false;
    }

    ctx.uploadWorldAndDynamicEntities(world, true);

    SceneUniforms u{};
    fillSceneUniforms(world, width, height, 0.5f, u);
    wgpuQueueWriteBuffer(ctx.queue, ctx.uniformBuffer, 0, &u, sizeof(SceneUniforms));

    const uint32_t unpaddedBytesPerRow = width * 4u;
    const uint32_t paddedBytesPerRow   = (unpaddedBytesPerRow + 255u) & ~255u;
    const size_t readbackSize          = static_cast<size_t>(paddedBytesPerRow) * height;

    WGPUTextureDescriptor outTexDesc{};
    outTexDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    outTexDesc.dimension = WGPUTextureDimension_2D;
    outTexDesc.size = WGPUExtent3D{width, height, 1};
    outTexDesc.format = WGPUTextureFormat_RGBA8Unorm;
    outTexDesc.mipLevelCount = 1;
    outTexDesc.sampleCount = 1;
    WGPUTexture outTex = wgpuDeviceCreateTexture(ctx.device, &outTexDesc);
    WGPUTextureView outView = wgpuTextureCreateView(outTex, nullptr);

    WGPUBufferDescriptor readDesc{};
    readDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    readDesc.size = readbackSize;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(ctx.device, &readDesc);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(ctx.device, nullptr);
    ctx.recordFramePasses(encoder, outView);

    WGPUTexelCopyTextureInfo srcCopy{};
    srcCopy.texture = outTex;
    srcCopy.mipLevel = 0;
    srcCopy.origin = WGPUOrigin3D{0, 0, 0};
    srcCopy.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstCopy{};
    dstCopy.buffer = readBuffer;
    dstCopy.layout.offset = 0;
    dstCopy.layout.bytesPerRow = paddedBytesPerRow;
    dstCopy.layout.rowsPerImage = height;

    WGPUExtent3D copyExtent{width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcCopy, &dstCopy, &copyExtent);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(ctx.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(encoder);

    bool mapComplete = false;
    bool mapSuccess  = false;
    struct MapWaitState {
        bool* done;
        bool* ok;
    } waitState{&mapComplete, &mapSuccess};

    WGPUBufferMapCallbackInfo mapCb{};
    mapCb.mode = WGPUCallbackMode_AllowProcessEvents;
    mapCb.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* u1, void*) {
        auto* st = static_cast<MapWaitState*>(u1);
        *st->ok = (status == WGPUMapAsyncStatus_Success);
        *st->done = true;
    };
    mapCb.userdata1 = &waitState;

    wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, readbackSize, mapCb);
    while (!mapComplete) {
#if !defined(__EMSCRIPTEN__)
        wgpuDevicePoll(ctx.device, true, nullptr);
#endif
        wgpuInstanceProcessEvents(ctx.instance);
    }

    std::vector<uint8_t> rgbaPixels(static_cast<size_t>(width) * height * 4);
    if (mapSuccess) {
        const auto* mapped = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(readBuffer, 0, readbackSize));
        for (uint32_t y = 0; y < height; ++y) {
            std::memcpy(&rgbaPixels[static_cast<size_t>(y) * width * 4],
                        mapped + static_cast<size_t>(y) * paddedBytesPerRow,
                        static_cast<size_t>(width) * 4);
        }
        wgpuBufferUnmap(readBuffer);
    }

    wgpuBufferDestroy(readBuffer);
    wgpuBufferRelease(readBuffer);
    wgpuTextureViewRelease(outView);
    wgpuTextureDestroy(outTex);
    wgpuTextureRelease(outTex);
    ctx.shutdown();

    if (!mapSuccess) return false;
    return writePngFile(outputPngPath, width, height, rgbaPixels.data());
}

int ChartRenderer::runInteractiveWindow(
    ChartWorld& world,
    ChartScriptEngine& scriptEngine,
    const std::string& watchedScriptPath,
    AudioFoleyEngine& foleyEngine,
    uint32_t width,
    uint32_t height,
    FrameUpdateCallback frameCallback
) {
    if (!glfwInit()) {
        std::cerr << "[ChartRenderer] Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
        "Eatscharted - 2.5D Living Cartography Engine (Perilous Shores x Cat Quest)", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    GpuChartContext ctx;
    if (!ctx.init(window, width, height, world.inkAtlas())) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    world.updateSimulation(0.016f, width, height);
    ctx.uploadWorldAndDynamicEntities(world, true);

    std::string activeScript = watchedScriptPath;
    std::filesystem::file_time_type lastScriptWriteTime{};
    if (!activeScript.empty() && std::filesystem::exists(activeScript)) {
        lastScriptWriteTime = std::filesystem::last_write_time(activeScript);
    }

    std::cout << "\n====================================================================\n"
              << "  Eatscharted: 2.5D Living Cartography (Perilous Shores x Cat Quest)\n"
              << "====================================================================\n"
              << "  W / A / S / D or Arrows : Move Hero / Sail Caravel Across Map\n"
              << "  Left Shift              : Cat Quest Dodge-Roll / Sprint\n"
              << "  Space / J / Left Click  : Melee Sword Slash\n"
              << "  K / Right Click         : Cast Arcane Nova (Map-Surface Telegraph)\n"
              << "  E / Enter               : Enter / Exit Watabou One-Page Dungeon\n"
              << "  M / Z                   : Toggle Zoom-to-Atlas Cartographic View\n"
              << "  1 / 2 / 3 / 4           : Switch Chart (1: Perilous Shores, 2: Shattered Isles,\n"
              << "                            3: Highland Kingdom, 4: Sunken Sanctum Dungeon)\n"
              << "====================================================================\n\n";

    double lastTime = glfwGetTime();
    bool wasSpace = false, wasLeftClick = false, wasRightClick = false, wasK = false;
    bool wasE = false, wasM = false, was1 = false, was2 = false, was3 = false, was4 = false;
    bool atlasMode = false;
    float stepSoundCooldown = 0.0f;

    auto switchScene = [&](const std::string& path) {
        if (std::filesystem::exists(path) && scriptEngine.executeFile(path)) {
            activeScript = path;
            lastScriptWriteTime = std::filesystem::last_write_time(path);
            world.updateSimulation(0.016f, ctx.targetWidth, ctx.targetHeight);
            ctx.uploadWorldAndDynamicEntities(world, true);
            std::cout << "[Eatscharted] Loaded chart recipe: " << path << "\n";
        }
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        if (dt < 0.0001f) dt = 0.0001f;
        if (dt > 0.1f) dt = 0.1f;
        lastTime = now;

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW > 0 && fbH > 0 && (static_cast<uint32_t>(fbW) != ctx.targetWidth || static_cast<uint32_t>(fbH) != ctx.targetHeight)) {
            ctx.configureSurface(static_cast<uint32_t>(fbW), static_cast<uint32_t>(fbH));
            ctx.reallocateViewportTargets(static_cast<uint32_t>(fbW), static_cast<uint32_t>(fbH));
        }

        // Live Eatscript hot-reload check
        if (!activeScript.empty() && std::filesystem::exists(activeScript)) {
            auto curWrite = std::filesystem::last_write_time(activeScript);
            if (curWrite != lastScriptWriteTime) {
                lastScriptWriteTime = curWrite;
                if (scriptEngine.executeFile(activeScript)) {
                    world.updateSimulation(dt, ctx.targetWidth, ctx.targetHeight);
                    ctx.uploadWorldAndDynamicEntities(world, true);
                    std::cout << "[Eatscharted] Hot-reloaded '" << activeScript << "'\n";
                }
            }
        }

        // Scene preset switching (1, 2, 3, 4)
        bool k1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
        if (k1 && !was1) switchScene("scenes/perilous_shores.eats");
        was1 = k1;

        bool k2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
        if (k2 && !was2) switchScene("scenes/shattered_isles.eats");
        was2 = k2;

        bool k3 = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
        if (k3 && !was3) switchScene("scenes/highland_kingdom.eats");
        was3 = k3;

        bool k4 = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;
        if (k4 && !was4) switchScene("scenes/sunken_sanctum_dungeon.eats");
        was4 = k4;

        // Cat Quest Tabletop Hero Movement
        float mx = 0.0f, mz = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    mz += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  mz -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) mx += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  mx -= 1.0f;
        bool dodgeKey = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

        bool wasSailing = world.hero().isSailing;
        world.moveHero(mx, mz, dodgeKey, dt);
        if (world.hero().isSailing != wasSailing) {
            foleyEngine.triggerSound(FoleySound::OceanSplash, 0.9f);
        }

        stepSoundCooldown = std::max(0.0f, stepSoundCooldown - dt);
        if ((std::abs(mx) > 0.01f || std::abs(mz) > 0.01f) && stepSoundCooldown <= 0.0f) {
            foleyEngine.triggerSound(world.hero().isSailing ? FoleySound::OceanSplash : FoleySound::ParchmentStep, 0.35f);
            stepSoundCooldown = 0.28f;
        }

        // Melee Slash (Space / J / Left Click)
        bool spDown = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS);
        bool lcDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        if ((spDown && !wasSpace) || (lcDown && !wasLeftClick)) {
            world.triggerMeleeSlash();
            foleyEngine.triggerSound(FoleySound::SwordSlash, 0.85f);
        }
        wasSpace = spDown;
        wasLeftClick = lcDown;

        // Arcane Nova Spell (K / Right Click)
        bool kDown  = (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS);
        bool rcDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
        if ((kDown && !wasK) || (rcDown && !wasRightClick)) {
            world.triggerArcaneSpell();
            foleyEngine.triggerSound(FoleySound::ArcaneNova, 0.95f);
        }
        wasK = kDown;
        wasRightClick = rcDown;

        // Enter / Exit One-Page Dungeon (E / Enter)
        bool eDown = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS);
        if (eDown && !wasE) {
            if (world.tryToggleNearbyDungeon()) {
                foleyEngine.triggerSound(FoleySound::DungeonPortal, 1.0f);
                world.updateSimulation(dt, ctx.targetWidth, ctx.targetHeight);
                ctx.uploadWorldAndDynamicEntities(world, true);
            }
        }
        wasE = eDown;

        // Zoom-to-Atlas Toggle (M / Z)
        bool mDown = (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS);
        if (mDown && !wasM) {
            atlasMode = !atlasMode;
        }
        wasM = mDown;

        // Smoothly interpolate Cat Quest Tabletop Camera <-> High-Altitude Perilous Shores Atlas View
        auto& cam = world.camera();
        float targetAtlas = atlasMode ? 1.0f : 0.0f;
        cam.atlasZoom = std::lerp(cam.atlasZoom, targetAtlas, std::clamp(dt * 5.0f, 0.0f, 1.0f));

        float camHeight = std::lerp(28.0f, 82.0f, cam.atlasZoom);
        float camBackZ  = std::lerp(25.0f, 22.0f, cam.atlasZoom);
        Vec3f desiredTarget = world.hero().pos;
        Vec3f desiredPos    = {desiredTarget.x, desiredTarget.y + camHeight, desiredTarget.z - camBackZ};

        float camFollow = std::clamp(dt * 8.0f, 0.0f, 1.0f);
        cam.target.x   = std::lerp(cam.target.x,   desiredTarget.x, camFollow);
        cam.target.y   = std::lerp(cam.target.y,   desiredTarget.y, camFollow);
        cam.target.z   = std::lerp(cam.target.z,   desiredTarget.z, camFollow);
        cam.position.x = std::lerp(cam.position.x, desiredPos.x,    camFollow);
        cam.position.y = std::lerp(cam.position.y, desiredPos.y,    camFollow);
        cam.position.z = std::lerp(cam.position.z, desiredPos.z,    camFollow);
        cam.focusDistance = std::sqrt(camHeight * camHeight + camBackZ * camBackZ);

        bool rebuildStatic = false;
        if (frameCallback) {
            rebuildStatic = frameCallback(window, world, dt);
        }

        world.updateSimulation(dt, ctx.targetWidth, ctx.targetHeight);
        ctx.uploadWorldAndDynamicEntities(world, rebuildStatic);

        WGPUSurfaceTexture surfTex{};
        wgpuSurfaceGetCurrentTexture(ctx.surface, &surfTex);
        if (surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
            surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
            continue;
        }

        WGPUTextureView backbufferView = wgpuTextureCreateView(surfTex.texture, nullptr);

        SceneUniforms u{};
        fillSceneUniforms(world, ctx.targetWidth, ctx.targetHeight, static_cast<float>(now), u);
        wgpuQueueWriteBuffer(ctx.queue, ctx.uniformBuffer, 0, &u, sizeof(SceneUniforms));

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(ctx.device, nullptr);
        ctx.recordFramePasses(encoder, backbufferView);
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(ctx.queue, 1, &cmd);
        wgpuSurfacePresent(ctx.surface);

        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(backbufferView);
        wgpuTextureRelease(surfTex.texture);
    }

    ctx.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace eatscharted
