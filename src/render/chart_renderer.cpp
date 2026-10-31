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
#include <array>
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
    float gridOrigin[4]; // xy = activeChunkX, activeChunkZ, z = kChunkSize, w = kChunkRadius
    float discoveryGrid[16][4]; // 7x7 discovery grid packed in 16 vec4s
    float weatherParams[4]; // x = wetness, y = snowCover, z = stormSurge/windSpeed, w = lightningFlash
    float windDirection[4]; // xyz = windDirection, w = localWeatherType
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
    gridOrigin    : vec4<f32>,
    discoveryGrid : array<vec4<f32>, 16>,
    weatherParams : vec4<f32>,
    windDirection : vec4<f32>,
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

    let rawFoot = sp.posAndWidth.xyz;
    let basePos = applyHorizonCurve(rawFoot);
    let width   = sp.posAndWidth.w;
    let height  = sp.sizeAndMeta.x;
    let layer   = bitcast<u32>(sp.sizeAndMeta.y);
    let flags   = bitcast<u32>(sp.sizeAndMeta.z);
    let phase   = sp.sizeAndMeta.w;

    let corners = array<vec2<f32>, 6>(
        vec2<f32>(-0.5, 0.0),
        vec2<f32>( 0.5, 0.0),
        vec2<f32>( 0.5, 1.0),
        vec2<f32>(-0.5, 0.0),
        vec2<f32>( 0.5, 1.0),
        vec2<f32>(-0.5, 1.0)
    );
    let c = corners[cornerIdx];

    // Skip shadow casting for lightweight ground foliage (e.g. StandeeFlags::NoShadow = 128u)
    if ((flags & 128u) != 0u) {
        var skipOut : StandeeShadowOut;
        skipOut.clipPos  = vec4<f32>(0.0, 0.0, -2.0, 1.0);
        skipOut.uv       = vec2<f32>(0.0, 0.0);
        skipOut.texLayer = 0u;
        return skipOut;
    }

    var uCoord = c.x + 0.5;
    if ((flags & 1u) != 0u) {
        uCoord = 1.0 - uCoord;
    }
    let uv = vec2<f32>(uCoord, 1.0 - c.y);

    let time = u.camPos.w;
    var swayX = 0.0;
    var bobY  = 0.0;
    if ((flags & 2u) != 0u) {
        // Match vs_sprite wind breeze wave across foliage & grass
        let windWave = sin(time * 1.85 + rawFoot.x * 0.22 + rawFoot.z * 0.18 + phase);
        swayX = windWave * 0.12 * (c.y * c.y);
    }
    if ((flags & 4u) != 0u) {
        bobY = abs(sin(time * 2.8 + phase)) * 0.08 * c.y;
    }
    if ((flags & 16u) != 0u) {
        let toStandee = rawFoot.xz - u.heroPos.xz;
        let distH = length(toStandee);
        if (distH < 4.2 && rawFoot.z < u.heroPos.z + 0.5) {
            let pushDir = select(-1.0, 1.0, toStandee.x >= 0.0);
            swayX += pushDir * (1.0 - distH / 4.2) * 0.65 * c.y;
        }
    }

    let rightVec = normalize(vec3<f32>(u.camBasisX.x, 0.0, u.camBasisX.z));
    let tiltZ = select(0.0, 0.16 * c.y * height, (flags & 32u) != 0u);
    // Ground alignment: seat standee baseline at terrain elevation and sink base corners
    // slightly into ground so the shadow caster cleanly penetrates terrain with zero gap
    let groundSink = (3.0 / 64.0) * height;
    let baseDrop = select(0.06 * height + 0.12, 0.0, c.y > 0.5);
    let worldPos = basePos + rightVec * (c.x * width + swayX) + vec3<f32>(0.0, c.y * height + bobY - groundSink - baseDrop, tiltZ);

    var out : StandeeShadowOut;
    out.clipPos  = u.lightViewProj * vec4<f32>(worldPos, 1.0);
    out.uv       = uv;
    out.texLayer = layer;
    return out;
}

@fragment
fn fs_shadow_sprite(in : StandeeShadowOut) {
    var animUV = in.uv;
    if (in.texLayer == 27u && animUV.y < 0.22 && animUV.x > 0.51 && animUV.x < 0.76) {
        let flagDist = (animUV.x - 0.51) / 0.25;
        let wave = sin(u.camPos.w * 9.5 - animUV.x * 26.0) * (0.024 * flagDist);
        animUV.y += wave;
    }
    // Clamp UV.y to the art baseline (row 60) for the grounded base of standees
    let sampleUvY = min(animUV.y, 0.95);
    let texCoord = clamp(vec2<i32>(vec2<f32>(animUV.x, sampleUvY) * 64.0), vec2<i32>(0, 0), vec2<i32>(63, 61));
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
    gridOrigin    : vec4<f32>,
    discoveryGrid : array<vec4<f32>, 16>,
    weatherParams : vec4<f32>,
    windDirection : vec4<f32>,
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

fn sampleShadowPCF(worldPos : vec3<f32>, surfNormal : vec3<f32>, ndotl : f32) -> f32 {
    // Normal-offset bias: push sampling point slightly along the surface normal towards light
    // to cleanly eliminate surface self-shadow acne without disconnecting cast shadows from casters
    let normalBias = surfNormal * (0.040 + 0.035 * (1.0 - ndotl));
    let biasedPos  = worldPos + normalBias;
    let lightClip  = u.lightViewProj * vec4<f32>(biasedPos, 1.0);
    let ndc = lightClip.xyz / lightClip.w;
    let shadowUV = vec2<f32>(ndc.x * 0.5 + 0.5, -ndc.y * 0.5 + 0.5);
    if (shadowUV.x < 0.01 || shadowUV.x > 0.99 || shadowUV.y < 0.01 || shadowUV.y > 0.99 || ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }
    let texel = 1.0 / 2048.0;
    let depthBias = max(0.00035, 0.00080 * (1.0 - ndotl));
    let depthRef = ndc.z - depthBias;
    var sum = 0.0;
    for (var y = -1; y <= 1; y = y + 1) {
        for (var x = -1; x <= 1; x = x + 1) {
            let offset = vec2<f32>(f32(x), f32(y)) * texel;
            sum += textureSampleCompareLevel(shadowMap, shadowSampler, shadowUV + offset, depthRef);
        }
    }
    return sum / 9.0;
}

fn getChartDiscovery(mapXZ : vec2<f32>) -> f32 {
    let chunkSize = u.gridOrigin.z;
    let radius = u.gridOrigin.w;
    let dim = radius * 2.0 + 1.0;
    let minCorner = vec2<f32>(u.gridOrigin.x - radius, u.gridOrigin.y - radius) * chunkSize;
    let relPos = (mapXZ - minCorner) / chunkSize;

    var gridD = 0.0;
    if (relPos.x >= -0.2 && relPos.x <= (dim + 0.2) && relPos.y >= -0.2 && relPos.y <= (dim + 0.2)) {
        // Center the sampling at chunk centers (0.5, 1.5, 2.5, 3.5, 4.5)
        let samplePos = relPos - vec2<f32>(0.5, 0.5);
        let ix = clamp(floor(samplePos.x), 0.0, dim - 2.0);
        let iz = clamp(floor(samplePos.y), 0.0, dim - 2.0);
        let fx = clamp(samplePos.x - ix, 0.0, 1.0);
        let fz = clamp(samplePos.y - iz, 0.0, 1.0);
        let stride = i32(dim);

        let idx00 = i32(iz) * stride + i32(ix);
        let idx10 = i32(iz) * stride + i32(ix + 1.0);
        let idx01 = i32(iz + 1.0) * stride + i32(ix);
        let idx11 = i32(iz + 1.0) * stride + i32(ix + 1.0);

        let d00 = u.discoveryGrid[idx00 / 4][idx00 % 4];
        let d10 = u.discoveryGrid[idx10 / 4][idx10 % 4];
        let d01 = u.discoveryGrid[idx01 / 4][idx01 % 4];
        let d11 = u.discoveryGrid[idx11 / 4][idx11 % 4];

        gridD = mix(mix(d00, d10, fx), mix(d01, d11, fx), fz);

        // Smooth feathering at the outer perimeter of the active chunk grid (no rectangular cliff)
        let edgeDistX = min(relPos.x, dim - relPos.x);
        let edgeDistY = min(relPos.y, dim - relPos.y);
        let gridFade = smoothstep(-0.1, 0.45, min(edgeDistX, edgeDistY));
        gridD *= gridFade;
    }

    // Direct hero sight radius: 20m full sight, smooth organic fade to 32m
    let heroDist = length(mapXZ - u.heroPos.xz);
    let heroSight = 1.0 - smoothstep(20.0, 32.0, heroDist);

    // High-Altitude Atlas Zoom: reveal the macro continental map seamlessly without chunk boundaries
    let atlasZoom = u.heroPos.w;
    let atlasReveal = smoothstep(0.12, 0.75, atlasZoom);

    return clamp(max(mix(max(gridD, heroSight), 1.0, atlasReveal), 0.0), 0.0, 1.0);
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
    if (u.gridOrigin.w < 0.0) {
        // Watabou One-Page Dungeon Mode: Subterranean Rock Canvas matching dungeon surrounding pattern!
        // No open sky or stars in a subterranean dungeon!
        let rockBase = vec3<f32>(0.72, 0.66, 0.56);
        let inkColor = vec3<f32>(0.12, 0.08, 0.06);

        // Backdrop diagonal striping matching the surrounding pattern
        let h1 = abs(fract((in.uv.x + in.uv.y) * 16.0) - 0.5);
        let hatch1 = 1.0 - smoothstep(0.08, 0.20, h1);
        let dungeonBg = mix(rockBase, inkColor, hatch1 * 0.40);
        return vec4<f32>(dungeonBg, 240.0);
    }

    let t = clamp(in.uv.y, 0.0, 1.0);
    let grain = (fbmPaper(in.uv * 48.0) - 0.5) * 0.04;
    var skyCol = mix(u.skyZenith.rgb, u.skyHorizon.rgb, pow(t, 0.75)) + vec3<f32>(grain);

    // Subtle celestial stars in the nocturnal sky on antique parchment
    let isNight = smoothstep(0.28, 0.06, dot(u.skyZenith.rgb, vec3<f32>(0.299, 0.587, 0.114)));
    if (isNight > 0.01) {
        let starCoord = floor(in.uv * 170.0);
        let starRnd = hash2D(starCoord);
        let starTwinkle = sin(u.camPos.w * 2.4 + starRnd * 6.28) * 0.30 + 0.70;
        let star = step(0.984, starRnd) * starTwinkle * isNight * 0.42;
        skyCol += vec3<f32>(star * 0.75, star * 0.85, star);
    }

    // Flash illumination from lightning during storms
    if (u.weatherParams.w > 0.001) {
        skyCol += vec3<f32>(0.65, 0.78, 1.0) * (u.weatherParams.w * 1.6);
    }

    return vec4<f32>(skyCol, 240.0);
}

// Watabou Perilous Shores + Cat Quest Biome Watercolor Palette (Lush & Saturated)
fn biomePaperTint(biomeId : u32) -> vec3<f32> {
    switch (biomeId) {
        case 0u: { return vec3<f32>(0.055, 0.380, 0.460); } // OceanDeep (Rich Nautical Teal Navy)
        case 1u: { return vec3<f32>(0.220, 0.760, 0.710); } // CoastalShoals (Bright Tropical Turquoise)
        case 2u: { return vec3<f32>(0.965, 0.895, 0.585); } // SandyBeach (Warm Sunlit Golden Sand)
        case 3u: { return vec3<f32>(0.340, 0.620, 0.380); } // SaltMarsh (Lush Wetland Olive-Emerald)
        case 4u: { return vec3<f32>(0.440, 0.745, 0.220); } // LushMeadow (Vibrant Cat Quest Grassy Meadow #70be38)
        case 5u: { return vec3<f32>(0.220, 0.460, 0.170); } // DeciduousWoodland (Deep Emerald Forest Floor)
        case 6u: { return vec3<f32>(0.180, 0.380, 0.240); } // TaigaPines (Cool Boreal Pine Green)
        case 7u: { return vec3<f32>(0.920, 0.780, 0.340); } // AridSteppe (Warm Golden Savanna)
        case 8u: { return vec3<f32>(0.840, 0.480, 0.240); } // BadlandsCanyon (Vibrant Terracotta Canyon)
        case 9u: { return vec3<f32>(0.955, 0.975, 0.990); } // AlpinePeak (Crisp Snow Summit White)
        case 10u:{ return vec3<f32>(0.480, 0.280, 0.520); } // CalamityBlight (Deep Arcane Violet)
        case 11u:{ return vec3<f32>(0.84, 0.79, 0.69); }    // DungeonFlagstone
        case 12u:{ return vec3<f32>(0.72, 0.66, 0.56); }    // DungeonHatchedWall
        case 13u:{ return vec3<f32>(0.880, 0.930, 0.960); } // SnowyTundra (Pale frosty arctic tundra)
        case 14u:{ return vec3<f32>(0.760, 0.890, 0.950); } // GlacialIce (Glacial cerulean ice pack)
        default: { return vec3<f32>(0.440, 0.745, 0.220); }
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
    @location(7) discovery     : vec2<f32>, // x = rawDisc, y = charted
    @location(8) terrainColor  : vec4<f32>, // rgb = continuous watercolor tint, a = vegetation factor
    @location(9) terrainParams : vec4<f32>, // x = arid factor, y = blend weight, z = unused, w = unused
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

    let biome0 = out.packedBiomes & 255u;
    let biome1 = (out.packedBiomes >> 8u) & 255u;
    let blend  = clamp(v.gradRiverBlend.w, 0.0, 1.0);

    // Direction C: Hardware Attribute Interpolation for continuous terrain color
    // For overland terrain, ensure underwater vertices represent coastal sand on dry land
    // so dry land fragments in shoreline quads never inherit ocean deep/shoal turquoise.
    var landBiome0 = biome0;
    if (landBiome0 <= 1u) {
        landBiome0 = 2u; // SandyBeach
    }
    var landBiome1 = biome1;
    if (landBiome1 <= 1u) {
        landBiome1 = 2u; // SandyBeach
    }

    var baseCol0 = biomePaperTint(landBiome0);
    var baseCol1 = biomePaperTint(landBiome1);
    var baseCol  = baseCol0;
    if (landBiome0 != landBiome1) {
        baseCol = mix(baseCol0, baseCol1, blend);
    }

    // Continuous vegetation factor (1.0 = meadow/forest/taiga, 0.0 = beach/steppe/badlands/peak)
    var veg0 = 0.0;
    if (landBiome0 == 4u || landBiome0 == 5u || landBiome0 == 6u) {
        veg0 = 1.0;
    } else if (landBiome0 == 3u) {
        veg0 = 0.40;
    }
    var veg1 = 0.0;
    if (landBiome1 == 4u || landBiome1 == 5u || landBiome1 == 6u) {
        veg1 = 1.0;
    } else if (landBiome1 == 3u) {
        veg1 = 0.40;
    }
    let vegFactor = mix(veg0, veg1, blend);

    // Continuous arid factor (for AridSteppe & BadlandsCanyon)
    var arid0 = 0.0;
    if (landBiome0 == 7u || landBiome0 == 8u) {
        arid0 = 1.0;
    }
    var arid1 = 0.0;
    if (landBiome1 == 7u || landBiome1 == 8u) {
        arid1 = 1.0;
    }
    let aridFactor = mix(arid0, arid1, blend);

    // Continuous snowy tundra factor (BiomeId::SnowyTundra = 13u)
    var tundra0 = 0.0;
    if (landBiome0 == 13u) {
        tundra0 = 1.0;
    }
    var tundra1 = 0.0;
    if (landBiome1 == 13u) {
        tundra1 = 1.0;
    }
    let tundraFactor = mix(tundra0, tundra1, blend);

    // Continuous glacial ice factor (BiomeId::GlacialIce = 14u)
    var ice0 = 0.0;
    if (landBiome0 == 14u) {
        ice0 = 1.0;
    }
    var ice1 = 0.0;
    if (landBiome1 == 14u) {
        ice1 = 1.0;
    }
    let iceFactor = mix(ice0, ice1, blend);

    out.terrainColor  = vec4<f32>(baseCol, vegFactor);
    out.terrainParams = vec4<f32>(aridFactor, blend, tundraFactor, iceFactor);

    // Apply hardware depth bias to ground-inscribed map banners & path text so they NEVER z-fight or clip into terrain
    if ((out.flags & 8u) != 0u) {
        out.clipPos.z -= 0.0015 * out.clipPos.w;
    }

    let rawDisc = getChartDiscovery(rawPos.xz);
    let charted = smoothstep(0.35, 0.65, rawDisc);
    out.discovery = vec2<f32>(rawDisc, charted);
    return out;
}
)";

static const char* kSceneWgslPart2a = R"(
@fragment
fn fs_world(in : WorldVSOut) -> @location(0) vec4<f32> {
    let inkColor    = vec3<f32>(0.12, 0.09, 0.07);
    let inkSoft     = vec3<f32>(0.26, 0.20, 0.16);
    let inkDark     = vec3<f32>(0.09, 0.07, 0.05);
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

    var waveFoam = 0.0;
    var isWater  = 0.0;
    var surfNormal = normalize(in.normal);

    // 0. Check if this vertex belongs to a 3D Curved Calligraphic Map Banner!
    if ((in.flags & 8u) != 0u) {
        let bDisc = in.discovery.x;
        let bCharted = smoothstep(0.25, 0.55, bDisc);

        var bannerInk = vec3<f32>(0.10, 0.07, 0.05); // Crisp India ink lettering
        if (biome0 == 0u && biome1 == 0u) {
            bannerInk = vec3<f32>(0.22, 0.16, 0.12) * 0.40; // Soft ink drop shadow on parchment
        } else if (biome0 == 0u) {
            bannerInk = vec3<f32>(0.14, 0.22, 0.32); // Nautical blue ink
        }
        let pencilDraft = vec3<f32>(0.65, 0.60, 0.54) * 0.45;
        bannerInk = mix(pencilDraft, bannerInk, bCharted);

        let camDist = length(in.worldPos - u.camPos.xyz);
        return vec4<f32>(bannerInk, camDist);
    }

    // 1. Multi-Scale Organic Procgen Noise & Micro-Surface Perturbation (skipped in charted Dungeon Mode)
    var col = in.terrainColor.rgb;
    var mediumNoise = 0.5;
    var fineGrain   = 0.5;
    var microGrain  = 0.5;

    if ((in.flags & 16u) == 0u || in.discovery.x < 0.995) {
        let macroNoise = fbmPaper(mapXZ * 0.42);
        mediumNoise    = fbmPaper(mapXZ * 1.85 + vec2<f32>(macroNoise * 1.4, -macroNoise * 1.1));
        fineGrain      = valueNoise2D(mapXZ * 9.5);
        microGrain     = valueNoise2D(mapXZ * 24.0);
    }

    if ((in.flags & 16u) == 0u) {
        // Organic watercolor pigment bleed & drying-edge wash feathering (Direction A)
        let edgeBlend = in.terrainParams.y;
        if (edgeBlend > 0.04 && edgeBlend < 0.96) {
            let dryingEdge = 1.0 - abs(edgeBlend - 0.50) * 2.0;
            let pigmentConcentration = dryingEdge * (0.05 + 0.06 * mediumNoise);
            col *= (1.0 - pigmentConcentration);
        }
        col *= (0.86 + 0.18 * mediumNoise + 0.08 * (fineGrain - 0.5) + 0.04 * (microGrain - 0.5));

        // Perturb terrain surface normal with organic micro-relief so sunlight catches turf & soil grain
        let microNx = (fineGrain - 0.5) * 0.16 + (microGrain - 0.5) * 0.08;
        let microNz = (mediumNoise - 0.5) * 0.16 - (fineGrain - 0.5) * 0.08;
        surfNormal = normalize(surfNormal + vec3<f32>(microNx, 0.0, microNz));
    }

    // 2. Watabou One-Page Dungeon Mode (2.5D Raised Walls, Inked Ashlar Masonry, Sunken Pools & Gridded Flagstones)
    if ((in.flags & 16u) != 0u) {
        let rockBase = vec3<f32>(0.72, 0.66, 0.56);
        let flagBase = vec3<f32>(0.84, 0.79, 0.69);

        // Vector-accurate Analytical Signed Distance Field evaluated per-pixel from mapXZ!
        // This eliminates 100% of 1m grid discretization or diagonal triangle artifacts!
        var analyticalDist = 100.0;
        let roomCount = u32(u.discoveryGrid[11].x);
        for (var ri = 0u; ri < roomCount; ri = ri + 1u) {
            let rm = u.discoveryGrid[ri];
            let cx = 0.5 * (rm.x + rm.z);
            let cz = 0.5 * (rm.y + rm.w);
            let hx = 0.5 * (rm.z - rm.x);
            let hz = 0.5 * (rm.w - rm.y);
            let dP = abs(mapXZ - vec2<f32>(cx, cz)) - vec2<f32>(hx, hz);
            let dOut = length(max(dP, vec2<f32>(0.0)));
            let dIn  = min(max(dP.x, dP.y), 0.0);
            analyticalDist = min(analyticalDist, dOut + dIn);
        }

        let corrCount = u32(u.discoveryGrid[11].y);
        for (var ci = 0u; ci < corrCount; ci = ci + 1u) {
            let corr = u.discoveryGrid[6u + ci];
            let p0 = corr.xy;
            let p1 = corr.zw;
            let v = p1 - p0;
            let lSq = dot(v, v);
            var t = 0.0;
            if (lSq > 0.0001) {
                t = clamp(dot(mapXZ - p0, v) / lSq, 0.0, 1.0);
            }
            let p = p0 + t * v;
            let d = length(mapXZ - p) - 1.8;
            analyticalDist = min(analyticalDist, d);
        }

        // Distance to carved rooms: dRoom < 0 = inside room floor, dRoom > 0 = wall/surroundings
        let dRoom = select(-sdfCoast, analyticalDist, roomCount > 0u);
        let isFloor = smoothstep(0.04, -0.04, dRoom);

        // Heavy dark India-ink wall casing perimeter stroke (Watabou style)
        let wallBorderStroke = 1.0 - smoothstep(0.04, 0.22, abs(dRoom));

        // 2.5D Raised Wall Shading: Ashlar masonry block courses along wall face (dRoom in [0.0, 1.4m])
        let inWallFace = smoothstep(0.0, 0.12, dRoom) * (1.0 - smoothstep(1.25, 1.45, dRoom));
        
        // Horizontal ashlar block courses & vertical stone masonry joints
        let courseY = fract(in.worldPos.y * 2.8);
        let ashlarMortar = 1.0 - smoothstep(0.04, 0.14, courseY);
        let stoneStagger = floor(in.worldPos.y * 2.8) * 0.5;
        let ashlarJoint = 1.0 - smoothstep(0.04, 0.14, abs(fract((mapXZ.x + mapXZ.y) * 0.75 + stoneStagger) - 0.5));
        let masonryStroke = clamp(ashlarMortar + ashlarJoint * 0.65, 0.0, 1.0);

        // Cross-hatching for solid rock outside room (dRoom >= 0.0)
        let h1 = abs(fract((mapXZ.x + mapXZ.y) * 1.85) - 0.5);
        let h2 = abs(fract((mapXZ.x - mapXZ.y) * 1.85) - 0.5);
        let hatch1 = 1.0 - smoothstep(0.08, 0.18, h1);
        let denseNearWall = smoothstep(4.2, 0.0, dRoom);
        let hatch2 = (1.0 - smoothstep(0.08, 0.18, h2)) * denseNearWall;
        let rockInkW = clamp(hatch1 * 0.65 + hatch2 * 0.60, 0.0, 1.0);
        var rockCol = mix(rockBase, inkColor, rockInkW);

        // Apply ashlar masonry & shadow depth on the 2.5D wall face
        if (inWallFace > 0.01) {
            let wallFaceBase = mix(rockBase * 0.88, inkDark, masonryStroke * 0.42);
            rockCol = mix(rockCol, wallFaceBase, inWallFace * 0.85);
        }

        // 2m flagstone grid tiles inside room (dRoom < 0.0)
        let tileX = abs(fract(mapXZ.x * 0.5 + 0.5) - 0.5) * 2.0;
        let tileZ = abs(fract(mapXZ.y * 0.5 + 0.5) - 0.5) * 2.0;
        let seam = 1.0 - smoothstep(0.0, 0.06, min(tileX, tileZ));
        let wallShadow = exp(-max(0.0, -dRoom) * 1.25) * 0.35;
        var flagstoneCol = mix(flagBase * (1.0 - wallShadow), inkSoft, seam * 0.45);

        // Subterranean flooded cistern pool (Vector-Smooth Analytical SDF Evaluation, ZERO Triangles!)
        var waterDist = 100.0;
        let poolCount = u32(u.discoveryGrid[11].z);
        for (var pi = 0u; pi < poolCount; pi = pi + 1u) {
            let wp = u.discoveryGrid[12u + pi];
            let poolCenter = (wp.xy + wp.zw) * 0.5;
            let poolHalf   = (wp.zw - wp.xy) * 0.5;
            let dPoolP     = abs(mapXZ - poolCenter) - poolHalf;
            let dOut       = length(max(dPoolP, vec2<f32>(0.0)));
            let dIn        = min(max(dPoolP.x, dPoolP.y), 0.0);
            waterDist = min(waterDist, dOut + dIn);
        }
        if (poolCount == 0u) {
            waterDist = in.sdfs.z;
        }

        let inPool = 1.0 - smoothstep(-0.06, 0.06, waterDist);
        if (inPool > 0.001) {
            let waterBase = vec3<f32>(0.10, 0.42, 0.48); // Deep antique teal
            let ripple = sin(time * 2.2 + (mapXZ.x + mapXZ.y) * 1.6) * 0.07 +
                         cos(time * 1.7 + (mapXZ.x - mapXZ.y) * 1.4) * 0.04;
            let waterPoolCol = waterBase + vec3<f32>(ripple * 0.5, ripple * 0.8, ripple);

            // Inked stone coping rim separating flagstones from water
            let poolRimStroke = 1.0 - smoothstep(0.04, 0.22, abs(waterDist));
            let waterWithRim = mix(waterPoolCol, inkColor, poolRimStroke * 0.90);

            // Sunken basin depth shading (darker towards center of pool)
            let basinDepth = clamp(-waterDist / 1.8, 0.0, 1.0);
            let shadedWater = mix(waterWithRim, vec3<f32>(0.04, 0.24, 0.28), basinDepth * 0.45);

            flagstoneCol = mix(flagstoneCol, shadedWater, inPool);
            isWater = inPool;
        }

        // Blend floor and solid rock based on continuous vector SDF
        var dungeonCol = mix(rockCol, flagstoneCol, isFloor);

        // Apply crisp India-ink wall casing stroke on the boundary
        dungeonCol = mix(dungeonCol, inkColor, wallBorderStroke * 0.95);

        col = dungeonCol;
    }
)";

static const char* kSceneWgslPart2b = R"(
    else {
        // 3. Overworld: Animated Wave Water + Vector Shoreline Rings vs. Realistic Noise-Textured Land
        let jitter = (mediumNoise - 0.5) * 0.35;
        let coastD = sdfCoast + jitter;

        if (coastD < 0.0) {
            // OCEAN / BAY: Animated Dual-Octave Swell Waves + Vector Shoreline Rings!
            let shoreDist = -coastD;

            // Subtle 3D Gerstner/Multi-Harmonic Wave Normal with Invariant Phase Rates
            // (Phase rates are constant with respect to time to prevent instantaneous speed-up when stormSurge transitions!)
            let stormSurge = u.weatherParams.z;
            let wPhase1 = mapXZ.x * 0.52 + mapXZ.y * 0.38 + time * 1.45;
            let wPhase2 = mapXZ.x * 0.31 - mapXZ.y * 0.64 - time * 1.15;
            let wPhase3 = (mapXZ.x - mapXZ.y) * 1.15 + time * 2.10;
            // High-frequency wind-blown chop octave whose AMPLITUDE smoothly fades in with stormSurge
            let chopPhase = mapXZ.x * 1.42 - mapXZ.y * 1.18 + time * 2.85;

            let baseWaveH = sin(wPhase1) * 0.50 + sin(wPhase2) * 0.35 + cos(wPhase3) * 0.15;
            let chopH     = sin(chopPhase) * 0.28 * stormSurge;
            let waveH     = baseWaveH * (1.0 + 0.75 * stormSurge) + chopH;

            let baseDWdX  = cos(wPhase1) * 0.52 * 0.50 + cos(wPhase2) * 0.31 * 0.35 - sin(wPhase3) * 1.15 * 0.15;
            let baseDWdZ  = cos(wPhase1) * 0.38 * 0.50 - cos(wPhase2) * 0.64 * 0.35 + sin(wPhase3) * 1.15 * 0.15;
            let chopDWdX  = cos(chopPhase) * 1.42 * 0.28 * stormSurge;
            let chopDWdZ  = -cos(chopPhase) * 1.18 * 0.28 * stormSurge;
            let dWdX      = baseDWdX * (1.0 + 0.75 * stormSurge) + chopDWdX;
            let dWdZ      = baseDWdZ * (1.0 + 0.75 * stormSurge) + chopDWdZ;

            // Smoothly damp wave normal right at the shoreline edge, fuller swell offshore
            let swellStrength = smoothstep(0.0, 3.5, shoreDist) * 0.22;
            surfNormal = normalize(vec3<f32>(-dWdX * swellStrength, 1.0, -dWdZ * swellStrength));

            // Stepped multi-band water shelf colors modulated by subtle animated wave troughs/crests
            let seaDeep    = vec3<f32>(0.050, 0.360, 0.450); // Deep Nautical Teal Navy
            let shelfOuter = vec3<f32>(0.085, 0.510, 0.555); // Outer Shelf Tropical Teal
            let shelfMid   = vec3<f32>(0.135, 0.655, 0.635); // Mid Shelf Ocean Turquoise
            let shelfShoal = vec3<f32>(0.220, 0.780, 0.720); // Shallow Coastal Lagoon Turquoise

            let waveShift = waveH * 0.18;
            var waterBase = seaDeep;
            if (shoreDist + waveShift <= 2.2) {
                waterBase = shelfShoal;
            } else if (shoreDist + waveShift <= 5.2) {
                waterBase = shelfMid;
            } else if (shoreDist + waveShift <= 9.5) {
                waterBase = shelfOuter;
            }
            // Subtle sunlit caustic & swell crest tinting in the water body
            let caustic = smoothstep(0.25, 0.85, waveH * 0.5 + 0.5) * 0.06;
            col = waterBase + vec3<f32>(caustic * 0.6, caustic * 0.9, caustic);

            // Crisp vector ink shelf contour boundaries separating each water band
            let contour0 = 1.0 - smoothstep(0.0, 0.14, abs(shoreDist + waveShift - 2.2));
            let contour1 = 1.0 - smoothstep(0.0, 0.14, abs(shoreDist + waveShift - 5.2));
            let contour2 = 1.0 - smoothstep(0.0, 0.14, abs(shoreDist + waveShift - 9.5));
            let shelfBorders = max(contour0, max(contour1, contour2)) * 0.42;

            // Concentric shoreline ripple rings in clean seafoam aqua
            let waveAnim  = sin(time * 1.45 + mapXZ.x * 0.28 + mapXZ.y * 0.22) * 0.16 + waveH * 0.06;
            let ringFreq  = 0.82;
            let ringPhase = fract((shoreDist + waveAnim) * ringFreq);
            let ringLine  = 1.0 - smoothstep(0.0, 0.15, abs(ringPhase - 0.35));
            let shoreFade = exp(-shoreDist * 0.18) * smoothstep(0.15, 0.55, shoreDist);
            let foamColor = vec3<f32>(0.86, 0.99, 0.97);
            let openWaveCrest = smoothstep(0.68, 0.95, waveH) * smoothstep(2.0, 8.0, shoreDist) * 0.22;
            col = mix(col, foamColor, clamp(ringLine * shoreFade * 0.52 + openWaveCrest, 0.0, 1.0));
            waveFoam = ringLine * shoreFade + openWaveCrest;
            isWater = 1.0;

            let waterInk = clamp(shelfBorders, 0.0, 1.0);
            col = mix(col, inkDark, waterInk);
        } else {
            // LANDMASS: Natural Noise-Based Procgen Terrain Materials (No Vector Grid/Hachure Clutter)
            let slopeMag = length(gradXZ);

            // Dynamic Rain Wetness, Paper Saturation & Procedural Concentric Puddle Ink Rings
            let wetness = u.weatherParams.x;
            if (wetness > 0.01) {
                col *= (1.0 - 0.16 * wetness);
                if (wetness > 0.22 && slopeMag < 0.15) {
                    let pUV = mapXZ * 2.4;
                    let pCell = floor(pUV);
                    let pFract = fract(pUV) - 0.5;
                    let dropSeed = hash2D(pCell);
                    let dropTime = fract(time * 1.7 + dropSeed * 6.28);
                    let ringRadius = dropTime * 0.40;
                    let ringDist = abs(length(pFract) - ringRadius);
                    let ripple = (1.0 - smoothstep(0.0, 0.055, ringDist)) * (1.0 - dropTime);
                    col = mix(col, inkDark, ripple * wetness * 0.45);
                }
            }

            // A) Natural Wind-Rippled Coastal Sand Beach & Wet Shoreline Gradient
            let beachNoise = (mediumNoise - 0.5) * 0.95;
            let beachSpan  = 3.8 + beachNoise;
            if (coastD < beachSpan) {
                let beachFactor = 1.0 - smoothstep(beachSpan * 0.35, beachSpan, coastD);
                // Wet sand darkening right near the water's edge (coastD < 1.05)
                let wetSand = 1.0 - smoothstep(0.0, 1.05, coastD);
                let drySandCol = vec3<f32>(0.94, 0.86, 0.58) * (0.94 + 0.12 * fineGrain);
                let wetSandCol = vec3<f32>(0.66, 0.58, 0.38) * (0.95 + 0.08 * microGrain);
                let sandCol = mix(drySandCol, wetSandCol, wetSand * 0.80);
                let bleedEdge = smoothstep(beachSpan * 0.70, beachSpan, coastD);
                let organicBleed = clamp(beachFactor + (fineGrain - 0.5) * 0.14 * bleedEdge, 0.0, 1.0);
                col = mix(col, sandCol, organicBleed);
            }

            // B) Grass & Woodland Organic Turf Mottling (Continuous via in.terrainColor.a)
            let vegFactor = in.terrainColor.a;
            if (vegFactor > 0.02) {
                let turfPatch = smoothstep(0.35, 0.72, mediumNoise);
                let lushHighlight = vec3<f32>(0.52, 0.82, 0.24);
                let deepMoss      = vec3<f32>(0.18, 0.42, 0.16);
                let grassMod = mix(lushHighlight, deepMoss, (1.0 - turfPatch) * (1.0 - fineGrain));
                col = mix(col, grassMod, vegFactor * 0.28);
            }
)";

static const char* kSceneWgslPart2bb = R"(
            // C) Sorcery! Arid Steppes & Sand Dunes: Warm Golden Ochre Wash + Inked Slope Stippling
            let aridFactor = in.terrainParams.x;
            if (aridFactor > 0.02) {
                let duneSun = smoothstep(-0.10, 0.65, dot(surfNormal, normalize(u.sunDir.xyz)));
                let warmDuneOchre = vec3<f32>(0.96, 0.82, 0.44);
                let coolDuneSand  = vec3<f32>(0.76, 0.64, 0.42);
                let duneWash = mix(coolDuneSand, warmDuneOchre, duneSun);
                col = mix(col, duneWash, aridFactor * 0.52);

                let stippleUV = mapXZ * 3.4;
                let stippleCell = fract(stippleUV);
                let stippleDist = length(stippleCell - vec2<f32>(0.5, 0.5));
                let stippleDot = 1.0 - smoothstep(0.06, 0.16, stippleDist);
                // Ultra-lightweight single hash evaluation for zero-overhead stipple noise
                let stippleNoise = hash2D(floor(stippleUV));
                if (stippleNoise > 0.40 && slopeMag > 0.08) {
                    col = mix(col, inkDark, stippleDot * 0.40 * aridFactor);
                }
            }

            // D) Sorcery! Mountain Knife-Edge Ridge Inking & Alpine Snow Crests
            let rockBlend = clamp(smoothstep(0.20, 0.50, slopeMag) + smoothstep(4.0, 6.8, in.rawMapPos.y) * 0.65, 0.0, 1.0);
            if (rockBlend > 0.01) {
                let graniteLit  = vec3<f32>(0.64, 0.66, 0.70) * (0.90 + 0.15 * fineGrain);
                let basaltDark  = vec3<f32>(0.26, 0.28, 0.32);
                let rockCol     = mix(graniteLit, basaltDark, (1.0 - microGrain) * 0.25);
                col = mix(col, rockCol, rockBlend * 0.85);

                // 1. High Mountain Ridge Knife-Edge Inking (Performant continuous normal & elevation scalar)
                let ridgeCrest = (1.0 - surfNormal.y) * smoothstep(3.5, 6.5, in.rawMapPos.y);
                if (ridgeCrest > 0.15) {
                    let ridgeInk = smoothstep(0.30, 0.65, ridgeCrest) * (1.0 - smoothstep(0.0, 0.18, abs(fract(in.rawMapPos.y * 0.45) - 0.5)));
                    col = mix(col, inkDark, ridgeInk * 0.65);
                }

                // 2. Alpine Snow accumulation on high upward-facing ledges (crisp contrast & dynamic weather snowline)
                let snowBase = 5.2 - u.weatherParams.y * 2.6;
                let snowHeight = smoothstep(snowBase, snowBase + 1.4, in.rawMapPos.y + (mediumNoise - 0.5) * 0.6);
                let snowFlatness = smoothstep(0.38, 0.72, surfNormal.y);
                let snowMask = clamp(snowHeight * snowFlatness, 0.0, 1.0);
                let snowCol = vec3<f32>(0.97, 0.98, 1.0) * (0.95 + 0.05 * fineGrain);
                col = mix(col, snowCol, snowMask);
            }

            // Dynamic Winter Snow Accumulation on foothills and plateaus during blizzards
            let globalSnow = u.weatherParams.y;
            if (globalSnow > 0.30) {
                let footSnowLine = 4.6 - globalSnow * 2.2;
                let hFootSnow = smoothstep(footSnowLine, footSnowLine + 1.2, in.rawMapPos.y + (mediumNoise - 0.5) * 0.5);
                let flatFootSnow = smoothstep(0.42, 0.75, surfNormal.y);
                let lowSnow = clamp(hFootSnow * flatFootSnow * (globalSnow - 0.30) / 0.70, 0.0, 1.0);
                if (lowSnow > 0.005) {
                    let snowCol = vec3<f32>(0.97, 0.98, 1.0) * (0.95 + 0.05 * fineGrain);
                    col = mix(col, snowCol, lowSnow * 0.85);
                }
            }

            // E1) Sorcery! Glacial Ice Sheets: Translucent Cyan Wash, Specular Sun Glint & Crevasse Fissures
            let iceFactor = in.terrainParams.w;
            if (iceFactor > 0.01) {
                let deepAzureIce  = vec3<f32>(0.24, 0.58, 0.78);
                let surfaceIce    = vec3<f32>(0.72, 0.89, 0.96);
                let iceDepthShade = smoothstep(-0.25, 0.75, dot(surfNormal, normalize(u.sunDir.xyz)));
                let iceBaseCol    = mix(deepAzureIce, surfaceIce, iceDepthShade * 0.55 + 0.45);

                // Glassy specular glint catching directional sunlight on the polished ice sheet
                let viewDir = normalize(u.camPos.xyz - in.worldPos);
                let sunReflect = reflect(-normalize(u.sunDir.xyz), surfNormal);
                let iceSpecular = pow(max(0.0, dot(sunReflect, viewDir)), 22.0) * 0.42;
                let iceShine = vec3<f32>(0.92, 0.98, 1.0) * iceSpecular;

                // Cartographic Glacial Crevasses: procedural fracture lines in crisp India ink with cerulean subsurface scatter
                let crevUV = mapXZ * 0.52;
                let crevNoise = fbmPaper(crevUV + vec2<f32>(fineGrain * 0.15, -microGrain * 0.15));
                let crevFract = abs(fract(crevNoise * 3.4) - 0.5) * 2.0;
                let isCrevasse = 1.0 - smoothstep(0.0, 0.09, crevFract);
                let crevasseGlow = smoothstep(0.05, 0.22, crevFract) * (1.0 - smoothstep(0.22, 0.40, crevFract));
                let crevColor = mix(inkDark, vec3<f32>(0.12, 0.72, 0.92), crevasseGlow * 0.75);

                var iceResult = mix(iceBaseCol, crevColor, isCrevasse * 0.75);
                iceResult += iceShine;
                col = mix(col, iceResult, iceFactor * 0.88);
            }

            // E2) Sorcery! Frost-Rimed Snowy Tundra: Directional Sastrugi Drifts, Permafrost Ground Polygons & Arctic Lichen
            let tundraFactor = in.terrainParams.z;
            if (tundraFactor > 0.01) {
                let snowParchment = vec3<f32>(0.965, 0.975, 0.995);
                let snowShadow    = vec3<f32>(0.760, 0.830, 0.910);

                // 1. Windblown Sastrugi (Snowdrift Waves) catching directional sunlight
                let sastrugiPhase = (mapXZ.x * 0.58 + mapXZ.y * 0.36) + (mediumNoise - 0.5) * 1.4;
                let sastrugiWave = sin(sastrugiPhase);
                let sastrugiShade = smoothstep(-0.25, 0.65, sastrugiWave) * 0.08;
                let sastrugiCol = mix(snowShadow, snowParchment, sastrugiShade + 0.5);

                // 2. Permafrost Ice-Wedge Polygons: Delicate geometric crack network on flat tundra
                let polyUV = mapXZ * 0.48;
                let polyCell = fract(polyUV) - 0.5;
                let polyDist = abs(max(abs(polyCell.x), abs(polyCell.y)) - 0.48);
                let polyCrack = 1.0 - smoothstep(0.0, 0.045, polyDist);
                let polyFlatness = 1.0 - smoothstep(0.12, 0.38, slopeMag);
                let polyInk = polyCrack * polyFlatness * 0.35;

                // 3. Windswept Arctic Lichen / Moss peeking through rocky tundra hummocks
                let exposedKnoll = smoothstep(0.60, 0.86, mediumNoise) * smoothstep(0.15, 0.42, slopeMag);
                let lichenCol = vec3<f32>(0.54, 0.58, 0.44) * (0.90 + 0.18 * fineGrain);

                var tundraResult = mix(sastrugiCol, lichenCol, exposedKnoll * 0.65);
                tundraResult = mix(tundraResult, inkDark, polyInk * 0.55);
                col = mix(col, tundraResult, tundraFactor * 0.90);
            }

            // G) Sorcery! Topographic Elevation Contour Lines on Hills, Mounds & Rolling Steppes
            let elevSpacing = 1.25;
            let elevFract = fract(in.rawMapPos.y / elevSpacing + (mediumNoise - 0.5) * 0.06);
            let contourDist = abs(elevFract - 0.5) * 2.0;
            let contourStroke = 1.0 - smoothstep(0.0, 0.065, contourDist);
            let contourMod = smoothstep(0.06, 0.28, slopeMag) * (1.0 - smoothstep(0.48, 0.78, slopeMag));
            let isHillOrSteppe = clamp(aridFactor * 1.5 + smoothstep(1.2, 4.2, in.rawMapPos.y), 0.0, 1.0);
            let contourInk = contourStroke * contourMod * (0.35 + 0.55 * isHillOrSteppe);
            col = mix(col, inkDark, contourInk * 0.62);

            // E) Winding Rivers with Multi-Tiered Depth Banding, Inked Shelf Contours & Downstream Current Flow
            if (sdfRiver < 0.35) {
                let riverW = max(0.5, in.gradAndUV.w);
                let riverFill = 1.0 - smoothstep(-0.15, 0.12, sdfRiver);
                let riverBank = (1.0 - smoothstep(0.0, 0.30, abs(sdfRiver))) * (1.0 - riverFill);

                // 1. Wet alluvial silt, clay & pebbles along the riverbank margins
                let wetSiltCol = vec3<f32>(0.34, 0.28, 0.20) * (0.88 + 0.15 * fineGrain);
                col = mix(col, wetSiltCol, riverBank * 0.70);

                // 2. Multi-tiered river depth color palette matching oceanic nautical tiers
                let depthNorm = clamp(-sdfRiver / riverW, 0.0, 1.0);
                let riverShoal = vec3<f32>(0.220, 0.780, 0.720); // Shallow shoal turquoise along banks
                let riverMid   = vec3<f32>(0.125, 0.630, 0.620); // Mid-current ocean turquoise
                let riverDeep  = vec3<f32>(0.055, 0.360, 0.440); // Deep channel trench navy

                var riverWater = riverDeep;
                if (depthNorm < 0.35) {
                    riverWater = riverShoal;
                } else if (depthNorm < 0.72) {
                    riverWater = riverMid;
                }

                // Crisp vector ink shelf contours separating river depth tiers (mirroring sea contours)
                let rShelf0 = 1.0 - smoothstep(0.0, 0.12, abs(depthNorm - 0.35));
                let rShelf1 = 1.0 - smoothstep(0.0, 0.12, abs(depthNorm - 0.72));
                let riverContours = max(rShelf0, rShelf1) * 0.40;

                // 3. Directional Downhill Current Flow
                let gradLen = length(in.gradAndUV.xy);
                let flowDir = select(vec2<f32>(0.0, -1.0), -in.gradAndUV.xy / max(0.001, gradLen), gradLen > 0.02);
                let perpDir = vec2<f32>(-flowDir.y, flowDir.x);

                // Longitudinal downstream coordinate & transverse channel coordinate
                let sLong  = dot(mapXZ, flowDir) - time * 2.8;
                let sCross = dot(mapXZ, perpDir);

                // Downstream traveling ripples & current streaks
                let flowWave = sin(sCross * 3.2) * sin(sLong * 2.2) * 0.08;
                let currentStreak = smoothstep(0.45, 0.92, sin(sLong * 1.6 + sin(sCross * 1.4) * 1.8)) * depthNorm * 0.22;

                // Cartographic current chevrons (directional flow indicators pointing downstream)
                let chevronPhase = fract(sLong * 0.40);
                let chevronDist  = abs(fract(sCross * 0.60) - 0.5) * 2.0 - chevronPhase;
                let chevronLine  = (1.0 - smoothstep(0.0, 0.12, abs(chevronDist))) * smoothstep(0.2, 0.85, depthNorm) * 0.20;

                let riverCol = riverWater + vec3<f32>(flowWave * 0.4, flowWave * 0.8, flowWave);
                col = mix(col, riverCol, riverFill);
                col = mix(col, inkDark, riverContours * riverFill);

                let riverFoamCol = vec3<f32>(0.86, 0.99, 0.97);
                col = mix(col, riverFoamCol, clamp(currentStreak + chevronLine, 0.0, 1.0) * riverFill);

                if (sdfRiver < 0.10) {
                    isWater = 1.0;
                    waveFoam = max(waveFoam, 0.20 + (currentStreak + chevronLine) * 1.6);
                    surfNormal = normalize(vec3<f32>(flowDir.x * flowWave * 1.8, 1.0, flowDir.y * flowWave * 1.8));
                }
            }

            // F) Trade Roads: Cobblestone/Dirt Highways & Worn Overland Footpaths
            if ((in.flags & 2u) != 0u) {
                let plank = abs(fract(roadArc * 1.8) - 0.5);
                let bridgeCol = mix(vec3<f32>(0.68, 0.52, 0.36), inkColor, step(0.42, plank) * 0.45);
                col = mix(col, bridgeCol, 1.0 - smoothstep(0.7, 1.1, sdfRoad));
            } else if (sdfRoad < 1.15) {
                let roadNoise = (fineGrain - 0.5) * 0.12;
                if (roadArc >= 0.0) {
                    // Cobblestone / packed earth highway with crisp ink borders
                    let roadFill = 1.0 - smoothstep(0.55, 0.75, sdfRoad + roadNoise);
                    let roadBorder = 1.0 - smoothstep(0.0, 0.10, abs(sdfRoad + roadNoise - 0.70));
                    let cobbleCol = vec3<f32>(0.84, 0.78, 0.64) * (0.92 + 0.16 * microGrain);
                    col = mix(col, cobbleCol, roadFill * 0.88);
                    col = mix(col, inkDark, roadBorder * 0.78);
                } else {
                    // Worn dirt & pebble overland trail
                    let trailFill = 1.0 - smoothstep(0.18, 0.55, sdfRoad + roadNoise);
                    let dirtCol = vec3<f32>(0.72, 0.58, 0.40) * (0.90 + 0.20 * fineGrain);
                    col = mix(col, dirtCol, trailFill * 0.82);
                }
            }
        }

        // Subtle shoreline water-edge grounding line at sdfCoast == 0
        let coastContour = 1.0 - smoothstep(0.0, 0.18, abs(coastD));
        col = mix(col, inkDark, coastContour * 0.55);
    }
)";

static const char* kSceneWgslPart2c = R"(
    // 3.5. Diegetic Fog-of-War & Ink Bloom Discovery Front (fast-path skipped when fully charted)
    let rawDisc = in.discovery.x;
    if (rawDisc < 0.995) {
        let paperBleed = (mediumNoise - 0.5) * 0.28;
        let charted = smoothstep(0.35, 0.65, rawDisc + paperBleed);

        // Uncharted vintage dry vellum base with delicate graphite pencil contour sketches
        let vellumBase = vec3<f32>(0.915, 0.865, 0.775) * (0.93 + 0.10 * mediumNoise + 0.03 * (fineGrain - 0.5));
        let pencilCoast = (1.0 - smoothstep(0.0, 0.22, abs(sdfCoast))) * 0.55;
        let pencilSlope = length(gradXZ) * 0.40;
        let pencilRoad  = (1.0 - smoothstep(0.0, 0.35, sdfRoad)) * 0.25;
        let pencilGraphite = vec3<f32>(0.38, 0.34, 0.30);
        let unchartedCol = mix(vellumBase, pencilGraphite, clamp(pencilCoast + pencilSlope + pencilRoad, 0.0, 1.0));

        // Transition uncharted pencil sketch vellum into rich vibrant world!
        col = mix(unchartedCol, col, charted);

        // Ink Bloom Capillary Edge: concentrated dark pigment fringe where wet watercolor blooms into dry paper!
        let inkBloomEdge = (1.0 - smoothstep(0.0, 0.15, abs(charted - 0.5))) * smoothstep(0.05, 0.95, rawDisc);
        let bloomPigment = vec3<f32>(0.16, 0.12, 0.08);
        col = mix(col, bloomPigment, inkBloomEdge * 0.55);
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

    // 5. Directional Celestial Sunlight / Moonlight & Dynamic Drop Shadows
    let N = surfNormal;
    let L = normalize(u.sunDir.xyz);
    let ndotl = max(dot(N, L), 0.0);

    // High-performance water wave distortion on cast shadows:
    // Distorts shadow sampling coordinates along wave slopes & surface ripples
    var shadowWorldPos = in.worldPos;
    if (isWater > 0.0) {
        let waveDistort = vec3<f32>(surfNormal.x * 0.55, (surfNormal.y - 1.0) * 0.35, surfNormal.z * 0.55) * isWater;
        shadowWorldPos += waveDistort;
    }
    let shadow = sampleShadowPCF(shadowWorldPos, N, ndotl);
    let shadowAmt = clamp(1.0 - shadow, 0.0, 1.0);

    // Hemispherical sky ambient lighting with rich day/night contrast
    let hemiWeight = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    let skyAmbient = mix(u.skyHorizon.rgb, u.skyZenith.rgb, hemiWeight);
    let ambientStrength = skyAmbient * 0.36;
    let directStrength = u.sunColor.rgb * (0.68 * ndotl * shadow);
    var lit = col * (ambientStrength + directStrength);

    // Momentary lightning strike illumination during storms
    let flash = u.weatherParams.w;
    if (flash > 0.001) {
        let lightningCol = vec3<f32>(0.85, 0.92, 1.15) * (flash * 2.2);
        lit += col * lightningCol * (shadow * 0.5 + 0.5);
    }

    // Dynamic water wave specular & glinting sun/moon highlights (drives Hexagonal Bokeh in Post-Process!)
    if (isWater > 0.5) {
        let V = normalize(u.camPos.xyz - in.worldPos);
        let H = normalize(L + V);
        let ndoth = max(dot(N, H), 0.0);
        let waterSpec = pow(ndoth, 42.0) * 1.45;
        let waveCrest = waveFoam * 0.65;
        let moonGlint = vec3<f32>(0.36, 0.55, 0.88);
        let celestialWater = mix(moonGlint * (waterSpec * 1.5 + waveCrest), u.sunColor.rgb * (waterSpec + waveCrest), clamp(u.sunColor.y * 2.0, 0.0, 1.0));
        lit += celestialWater * (shadow * 0.80 + 0.20);
    }

    // Soft ambient occlusion in cast drop shadows
    let shadowOcclude = ambientStrength * 0.30;
    lit = mix(lit, col * shadowOcclude, shadowAmt * 0.44);

    let lightCount = u32(u.camBasisY.w);
    for (var li = 0u; li < lightCount; li = li + 1u) {
        let pl = pointLights[li];
        let toL = pl.posAndRadius.xyz - in.rawMapPos;
        let d   = length(toL);
        let r   = pl.posAndRadius.w;
        if (d < r) {
            let atten = pow(1.0 - d / r, 2.0) * pl.colAndInt.w;
            lit += col * pl.colAndInt.rgb * atten * 0.65;
        }
    }

    let camDist = length(in.worldPos - u.camPos.xyz);
    let fogFactor = clamp(1.0 - exp(-camDist * u.sunDir.w), 0.0, 0.65);
    lit = mix(lit, u.skyHorizon.rgb, fogFactor);

    return vec4<f32>(lit, camDist);
}
)";

static const char* kSceneWgslPart3 = R"(
// 2.5D Volumetric Normal-Mapped Standees Pass (Realms-style 2.5D Trees, Grass, Rocks & Characters)
struct StandeeVSOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) worldPos      : vec3<f32>,
    @location(1) rawFootPos    : vec3<f32>,
    @location(2) uv            : vec2<f32>,
    @location(3) @interpolate(flat) texLayer : u32,
    @location(4) @interpolate(flat) flags    : u32,
    @location(5) charted       : vec2<f32>, // x = sDisc, y = finalCharted
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

    let corners = array<vec2<f32>, 6>(
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
        // Gentle wind breeze wave across foliage & grass, amplified during storms
        let windFactor = 0.85 + 0.45 * u.weatherParams.z;
        let windWave = sin(time * (1.85 + 0.75 * u.weatherParams.z) + rawFoot.x * 0.22 + rawFoot.z * 0.18 + phase);
        swayX = windWave * 0.12 * windFactor * (c.y * c.y);
    }
    if ((flags & 4u) != 0u) {
        bobY = abs(sin(time * 2.8 + phase)) * 0.08 * c.y;
    }

    // Proximity Canopy Parting: gently lean trees/foliage away when hero walks behind them!
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
    // Ground alignment: seat standee baseline cleanly on terrain
    let groundSink = (3.0 / 64.0) * height;
    let worldPos = basePos + rightVec * (c.x * width + swayX) + vec3<f32>(0.0, c.y * height + bobY - groundSink, tiltZ);

    var out : StandeeVSOut;
    out.clipPos    = u.viewProj * vec4<f32>(worldPos, 1.0);
    out.worldPos   = worldPos;
    out.rawFootPos = rawFoot;
    out.uv         = uv;
    out.texLayer   = layer;
    out.flags      = flags;

    let sDisc = getChartDiscovery(rawFoot.xz);
    var finalCharted = smoothstep(0.35, 0.65, sDisc);
    if (layer <= 3u) {
        finalCharted = 1.0;
    }
    out.charted = vec2<f32>(sDisc, finalCharted);
    return out;
}

@fragment
fn fs_sprite(in : StandeeVSOut) -> @location(0) vec4<f32> {
    var animUV = in.uv;
    // Performant cloth ripple for Caravel masthead pennant/flag in maritime breeze
    if (in.texLayer == 27u && animUV.y < 0.22 && animUV.x > 0.51 && animUV.x < 0.76) {
        let flagDist = (animUV.x - 0.51) / 0.25;
        let wave = sin(u.camPos.w * 9.5 - animUV.x * 26.0) * (0.024 * flagDist);
        animUV.y += wave;
    }
    let albedo = textureSampleLevel(albedoAtlas, atlasSampler, animUV, i32(in.texLayer), 0.0);
    if (albedo.a < 0.28) {
        discard;
    }

    // Screen-door stipple transparency if hero walks directly behind a tall canopy standee
    if ((in.flags & 16u) != 0u) {
        let distH = length(in.rawFootPos.xz - u.heroPos.xz);
        if (distH < 3.2 && in.rawFootPos.z < u.heroPos.z) {
            let checker = fract(in.clipPos.x * 0.5 + in.clipPos.y * 0.5);
            if (checker < 0.45) {
                discard;
            }
        }
    }

    let normSample = textureSampleLevel(normalAtlas, atlasSampler, animUV, i32(in.texLayer), 0.0);
    let emissive = normSample.a;

    // Reconstruct 3D world-space normal from the 2.5D normal atlas!
    var localN = normSample.xyz * 2.0 - vec3<f32>(1.0, 1.0, 1.0);
    if ((in.flags & 1u) != 0u) {
        localN.x = -localN.x; // Flip horizontal normal when sprite is X-flipped
    }
    let rightVec = normalize(vec3<f32>(u.camBasisX.x, 0.0, u.camBasisX.z));
    let upVec    = vec3<f32>(0.0, 1.0, 0.0);
    let fwdVec   = normalize(vec3<f32>(-u.camBasisZ.x, 0.25, -u.camBasisZ.z));
    let N        = normalize(rightVec * localN.x + upVec * localN.y + fwdVec * localN.z);
    let L        = normalize(u.sunDir.xyz);

    // Half-Lambert wrapped 2.5D diffuse + subsurface foliage translucency for trees & grass
    let rawNdotL = dot(N, L);
    let isFoliage = (in.texLayer >= 9u && in.texLayer <= 12u);
    let wrapDiff = clamp(rawNdotL * 0.62 + 0.38, 0.0, 1.0);
    let rockDiff = clamp(rawNdotL * 0.78 + 0.22, 0.0, 1.0);
    let ndotl    = select(rockDiff, wrapDiff, isFoliage);

    let shadow = sampleShadowPCF(in.worldPos, N, max(rawNdotL, 0.0));
    let shadowAmt = clamp(1.0 - shadow, 0.0, 1.0);

    // Root/ground contact ambient occlusion + hemispherical sky ambient
    let groundAO = smoothstep(0.0, 0.22, 1.0 - in.uv.y) * 0.22 + 0.78;
    let hemiWeight = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    let skyAmbient = mix(u.skyHorizon.rgb, u.skyZenith.rgb, hemiWeight);
    let ambientStrength = skyAmbient * (0.38 * groundAO);
    let directStrength  = u.sunColor.rgb * (0.76 * ndotl * shadow);

    // Warm golden-green subsurface scattering / rim glow when foliage is backlit or grazed by sun
    var subsurface = vec3<f32>(0.0);
    if (isFoliage) {
        let rim = pow(clamp(1.0 - localN.z, 0.0, 1.0), 1.6);
        subsurface = albedo.rgb * u.sunColor.rgb * (rim * 0.32 * shadow);
    }

    var lit = albedo.rgb * (ambientStrength + directStrength) + subsurface;
    let shadowOcclude = ambientStrength * 0.32;
    lit = mix(lit, albedo.rgb * shadowOcclude, shadowAmt * 0.38);

    // Standee 2.5D normal-aware point lighting (torches, streetlamps, campfires illuminating characters, trees & rocks!)
    let lightCount = u32(u.camBasisY.w);
    for (var li = 0u; li < lightCount; li = li + 1u) {
        let pl = pointLights[li];
        let toL = pl.posAndRadius.xyz - in.worldPos;
        let d   = length(toL);
        let r   = pl.posAndRadius.w;
        if (d < r) {
            let pDir  = toL / max(d, 0.001);
            let pDiff = clamp(dot(N, pDir) * 0.5 + 0.5, 0.2, 1.0);
            let atten = pow(1.0 - d / r, 2.0) * pl.colAndInt.w * pDiff;
            lit += albedo.rgb * pl.colAndInt.rgb * atten * 0.85;
        }
    }

    if ((in.flags & 8u) != 0u || emissive > 0.05) {
        lit = mix(lit, albedo.rgb * 1.55, max(emissive, 0.65));
    }

    // Combat Hit Flash (white/gold flash when struck by hero's blade or spell)
    if ((in.flags & 64u) != 0u) {
        lit = mix(lit, vec3<f32>(1.6, 1.4, 1.1), 0.72);
    }

    // Standee Fog-of-War: desaturates to graphite pencil lineart on aged vellum when uncharted!
    let sDisc = in.charted.x;
    let finalCharted = in.charted.y;

    let lum = dot(lit, vec3<f32>(0.299, 0.587, 0.114));
    let pencilStandee = vec3<f32>(lum * 0.82 + 0.14, lum * 0.78 + 0.11, lum * 0.68 + 0.08);
    lit = mix(pencilStandee, lit, finalCharted);

    let standeeBloom = (1.0 - smoothstep(0.0, 0.16, abs(finalCharted - 0.5))) * smoothstep(0.05, 0.95, sDisc);
    lit = mix(lit, vec3<f32>(0.15, 0.12, 0.08), standeeBloom * 0.50);

    let camDist = length(in.worldPos - u.camPos.xyz);
    let fogFactor = clamp(1.0 - exp(-camDist * u.sunDir.w), 0.0, 0.65);
    lit = mix(lit, u.skyHorizon.rgb, fogFactor);

    return vec4<f32>(lit, camDist);
}
)";

// ============================================================================
// WGSL SHADER 1b: CAMERA-FRUSTUM BOUNDED INSTANCED WEATHER PARTICLES
// ============================================================================
static const char* kSceneWgslPart4 = R"(
struct WeatherParticleOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) uv            : vec2<f32>,
    @location(1) color         : vec4<f32>,
    @location(2) @interpolate(flat) pType : u32,
};

@vertex
fn vs_weather_particles(@builtin(vertex_index) vertexIndex : u32) -> WeatherParticleOut {
    let particleId = vertexIndex / 6u;
    let cornerId   = vertexIndex % 6u;
    let time       = u.camPos.w;
    let intensity  = u.windDirection.y;
    let wType      = u.windDirection.w;

    let seedA = hash2D(vec2<f32>(f32(particleId) * 0.137, f32(particleId) * 0.389));
    let seedB = hash2D(vec2<f32>(f32(particleId) * 0.723, f32(particleId) * 0.915));
    let seedC = hash2D(vec2<f32>(f32(particleId) * 0.451, f32(particleId) * 0.283));

    // Dynamic Density Culling: continuous particle pool scaling by intensity
    if (seedA > intensity || intensity <= 0.005) {
        var out : WeatherParticleOut;
        out.clipPos = vec4<f32>(0.0, 0.0, -2.0, 1.0);
        out.uv = vec2<f32>(0.0);
        out.color = vec4<f32>(0.0);
        out.pType = 0u;
        return out;
    }

    let boxSize = vec3<f32>(48.0, 26.0, 48.0);
    let p0 = (vec3<f32>(seedA, seedB, seedC) - 0.5) * boxSize;

    var vel = vec3<f32>(0.0);
    var pWidth = 0.04;
    var pHeight = 0.65;
    var col = vec4<f32>(0.10, 0.13, 0.18, 0.38);
    var isType = 0u;

    let wind = u.windDirection.xyz * (3.0 + u.weatherParams.z * 12.0);

    if (wType == 5.0 || wType == 6.0) {
        // Snow / Flurries / Blizzard: Diegetic cut-paper flakes
        isType = 1u;
        let fallSpeed = select(1.6 + intensity * 1.4, 4.2, wType == 6.0);
        let wobbleX = sin(time * 3.2 + seedA * 6.28) * 0.65;
        let wobbleZ = cos(time * 2.8 + seedB * 6.28) * 0.65;
        vel = vec3<f32>(wind.x * 0.35 + wobbleX, -fallSpeed, wind.z * 0.35 + wobbleZ);

        // Flake scale & opacity smoothly scale with intensity:
        // Light snow (intensity ~0.15 - 0.25): delicate 0.06m - 0.10m flakes, soft gentle alpha
        // Blizzard (intensity ~1.0): 0.14m - 0.24m flakes, dense alpha
        let flakeScale = (0.06 + seedC * 0.08) * (0.55 + 0.45 * intensity);
        pWidth = flakeScale;
        pHeight = flakeScale;
        let brightness = 0.94 + seedA * 0.06;
        let alpha = (0.35 + 0.65 * intensity) * (0.80 + seedB * 0.20);
        col = vec4<f32>(brightness, brightness, 1.0, alpha);
    } else if (wType == 7.0) {
        // Sandstorm: Warm ochre dust flecks
        isType = 2u;
        vel = vec3<f32>(wind.x * 1.4, -1.2 + sin(time * 4.0 + seedA * 6.28) * 0.8, wind.z * 1.4);
        pWidth = (0.06 + seedC * 0.07) * (0.50 + 0.50 * intensity);
        pHeight = (0.05 + seedB * 0.05) * (0.50 + 0.50 * intensity);
        col = vec4<f32>(0.88, 0.74, 0.42, 0.30 + 0.55 * intensity);
    } else if (wType >= 2.0) {
        // Rain: Drizzle, Downpour, Thunderstorm (fast wind-sheared ink streaks)
        isType = 0u;
        let fallSpeed = select(14.0 + intensity * 6.0, 24.0, wType >= 3.0);
        vel = vec3<f32>(wind.x * 0.6, -fallSpeed, wind.z * 0.6);
        pWidth = (0.018 + 0.024 * intensity);
        pHeight = (0.28 + 0.55 * intensity);
        let alpha = (0.16 + 0.30 * intensity);
        col = vec4<f32>(0.10, 0.13, 0.18, alpha);
    } else {
        pWidth = 0.0;
        pHeight = 0.0;
    }

    let displaced = p0 + vel * time;
    let relToHero = displaced - u.heroPos.xyz;
    let wrapped = fract(relToHero / boxSize + 0.5) - 0.5;
    let particlePos = u.heroPos.xyz + wrapped * boxSize;
    let worldBase = applyHorizonCurve(particlePos);

    let corners = array<vec2<f32>, 6>(
        vec2<f32>(-0.5, -0.5),
        vec2<f32>( 0.5, -0.5),
        vec2<f32>( 0.5,  0.5),
        vec2<f32>(-0.5, -0.5),
        vec2<f32>( 0.5,  0.5),
        vec2<f32>(-0.5,  0.5)
    );
    let c = corners[cornerId];

    let rightVec = normalize(vec3<f32>(u.camBasisX.x, 0.0, u.camBasisX.z));
    let upVec    = normalize(vec3<f32>(u.camBasisY.x, u.camBasisY.y, u.camBasisY.z));

    var cornerWorld = worldBase;
    if (isType == 0u) {
        let streakDir = normalize(vel);
        let perp = select(vec3<f32>(0.0, 0.0, 1.0), vec3<f32>(1.0, 0.0, 0.0), abs(streakDir.z) > 0.9);
        let streakRight = normalize(cross(streakDir, perp));
        cornerWorld += streakRight * (c.x * pWidth) + streakDir * (c.y * pHeight);
    } else {
        let spinAngle = time * (1.8 + seedA * 2.2) + seedB * 6.28;
        let cosA = cos(spinAngle);
        let sinA = sin(spinAngle);
        let rotX = c.x * cosA - c.y * sinA;
        let rotY = c.x * sinA + c.y * cosA;
        cornerWorld += rightVec * (rotX * pWidth) + upVec * (rotY * pHeight);
    }

    var out : WeatherParticleOut;
    out.clipPos = u.viewProj * vec4<f32>(cornerWorld, 1.0);
    out.uv = c;
    out.color = col;
    out.pType = isType;
    return out;
}

@fragment
fn fs_weather_particles(in : WeatherParticleOut) -> @location(0) vec4<f32> {
    if (in.pType == 1u) {
        let d = abs(in.uv.x) + abs(in.uv.y);
        if (d > 0.48) { discard; }
        let edge = 1.0 - smoothstep(0.40, 0.48, d);
        return vec4<f32>(in.color.rgb, in.color.a * edge);
    } else if (in.pType == 2u) {
        let d = length(in.uv);
        if (d > 0.48) { discard; }
        let edge = 1.0 - smoothstep(0.25, 0.48, d);
        return vec4<f32>(in.color.rgb, in.color.a * edge);
    }
    let dX = abs(in.uv.x) * 2.0;
    let dY = abs(in.uv.y) * 2.0;
    let fade = (1.0 - smoothstep(0.1, 0.95, dX)) * (1.0 - smoothstep(0.65, 1.0, dY));
    return vec4<f32>(in.color.rgb, in.color.a * fade);
}
)";

// ============================================================================
// WGSL SHADER 2: REALMS-STYLE HEXAGONAL TILT-SHIFT BOKEH, GOD RAYS & ACES PASS
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
    gridOrigin    : vec4<f32>,
    discoveryGrid : array<vec4<f32>, 16>,
    weatherParams : vec4<f32>,
    windDirection : vec4<f32>,
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
    let pos = array<vec2<f32>, 3>(
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

// 6-Blade Hexagonal Camera Aperture Radius Factor for a given angle theta
// Modulates circular samples onto a true regular hexagon (with slight 15-deg camera lens tilt)
fn hexApertureScale(theta : f32) -> f32 {
    let bladeAngle = 1.04719755; // pi / 3 (60 degrees per hexagonal facet)
    let halfBlade  = 0.52359878; // pi / 6 (30 degrees)
    let localAng   = (theta + 0.261799) - floor((theta + 0.261799) / bladeAngle) * bladeAngle - halfBlade;
    return 0.8660254 / max(cos(localAng), 0.5);
}

@fragment
fn fs_post(in : PostVSOut) -> @location(0) vec4<f32> {
    let centerSample = textureSampleLevel(sceneTex, sceneSampler, in.uv, 0.0);
    let dist = centerSample.a;

    let focusDist  = u.skyZenith.w;
    let focusRange = max(u.skyHorizon.w, 1.0);
    let maxBokeh   = u.params.x * 1.25;

    // 1. Realms / Octopath HD-2D Miniature Tilt-Shift Hexagonal Bokeh Depth of Field
    let topTilt    = smoothstep(0.36, 0.01, in.uv.y) * 0.85;
    let bottomTilt = smoothstep(0.76, 0.99, in.uv.y) * 0.72;
    let depthCoc   = clamp((abs(dist - focusDist) - focusRange * 0.75) / focusRange, 0.0, 1.0);
    let coc        = clamp(max(depthCoc * 0.75, max(topTilt, bottomTilt)), 0.0, 1.0) * maxBokeh;

    let texelSize = 1.0 / vec2<f32>(max(u.params.y, 1.0), max(u.params.z, 1.0));
    var accum = centerSample.rgb;
    var wSum  = 1.0;

    // 24-Tap 6-Blade Hexagonal Aperture Bokeh Filter with Catadioptric Rim & HDR Specular Boost
    if (coc > 0.30) {
        // Ring 1 (6 inner hexagonal vertices) + Ring 2 (12 mid hexagonal facets) + Ring 3 (6 outer hex corners)
        for (var i = 1; i <= 24; i = i + 1) {
            let fi = f32(i);
            // Quantize angles & radii onto 6-blade hexagonal rings so bright highlights form crisp hexagons!
            var ringR = 0.45;
            var theta = fi * 1.04719755; // 60 deg steps
            if (i > 6 && i <= 18) {
                ringR = 0.78;
                theta = f32(i - 6) * 0.52359878; // 30 deg steps along hex edges
            } else if (i > 18) {
                ringR = 1.0;
                theta = f32(i - 18) * 1.04719755; // 60 deg outer hex vertices
            }
            let hexR = ringR * hexApertureScale(theta) * coc;
            let offset = vec2<f32>(cos(theta), sin(theta)) * hexR * texelSize;
            let tap = textureSampleLevel(sceneTex, sceneSampler, clamp(in.uv + offset, vec2<f32>(0.001), vec2<f32>(0.999)), 0.0);
            let tapLum = dot(tap.rgb, vec3<f32>(0.299, 0.587, 0.114));
            // Emphasize outer hexagonal aperture rim + bright water/torch/sun glints to create Realms hex bokeh
            let rimWeight = 0.85 + 0.35 * ringR;
            let highlightBoost = 1.0 + pow(clamp(tapLum - 0.68, 0.0, 3.0), 1.6) * 2.4 * ringR;
            let w = rimWeight * highlightBoost;
            accum += tap.rgb * w;
            wSum  += w;
        }
    } else {
        // 2. 4-Tap Subpixel Edge Anti-Aliasing (in-focus regions only, preserving crisp bokeh discs)
        let sU = textureSampleLevel(sceneTex, sceneSampler, in.uv + vec2<f32>(0.0, -texelSize.y), 0.0).rgb;
        let sD = textureSampleLevel(sceneTex, sceneSampler, in.uv + vec2<f32>(0.0,  texelSize.y), 0.0).rgb;
        let sL = textureSampleLevel(sceneTex, sceneSampler, in.uv + vec2<f32>(-texelSize.x, 0.0), 0.0).rgb;
        let sR = textureSampleLevel(sceneTex, sceneSampler, in.uv + vec2<f32>( texelSize.x, 0.0), 0.0).rgb;
        let crossAvg = (sU + sD + sL + sR) * 0.25;
        let edgeDiff = abs(sU - sD) + abs(sL - sR);
        let edgeContrast = dot(edgeDiff, vec3<f32>(0.299, 0.587, 0.114));
        let aaBlend = smoothstep(0.14, 0.48, edgeContrast) * 0.45;
        accum = mix(accum, crossAvg * wSum, aaBlend);
    }

    var hdrBase = (accum / wSum);

    // 3. Realms / Octopath Style Volumetric God Rays (Celestial Light Shafts)
    let sunVirtualWorld = u.camPos.xyz + u.sunDir.xyz * 160.0;
    let sunClip = u.viewProj * vec4<f32>(sunVirtualWorld, 1.0);
    var godRays = vec3<f32>(0.0);

    let isDaylight = smoothstep(0.08, 0.32, u.sunColor.y);
    if (sunClip.w > 0.0 && isDaylight > 0.01) {
        let sunNdc = sunClip.xyz / sunClip.w;
        let sunUV = vec2<f32>(sunNdc.x * 0.5 + 0.5, 0.5 - sunNdc.y * 0.5);

        let rayDelta = sunUV - in.uv;
        let rayDist = length(rayDelta);
        let rayDir = rayDelta / max(rayDist, 0.001);
        let maxStep = min(rayDist, 0.60);
        let stepVec = rayDir * (maxStep / 16.0);

        var rayCoord = in.uv;
        var raySum = 0.0;
        var decay = 1.0;

        for (var s = 0; s < 16; s = s + 1) {
            rayCoord += stepVec;
            let sTap = textureSampleLevel(sceneTex, sceneSampler, clamp(rayCoord, vec2<f32>(0.002), vec2<f32>(0.998)), 0.0);
            let tapLuma = dot(sTap.rgb, vec3<f32>(0.299, 0.587, 0.114));
            let isSky = smoothstep(95.0, 210.0, sTap.a);
            let brightGlint = smoothstep(1.10, 2.0, tapLuma) * 0.35;
            let rayGlint = max(isSky, brightGlint);
            raySum += rayGlint * decay;
            decay *= 0.91;
        }

        let radialFalloff = smoothstep(1.25, 0.05, rayDist);
        let sunVis = clamp(u.sunDir.y * 1.25, 0.0, 1.0);
        godRays = (raySum / 16.0) * u.sunColor.rgb * (0.14 * sunVis * isDaylight * radialFalloff);
    }

    // 4. Soft Atmospheric Bloom & Warm Light Spill
    let bloomLuma = max(0.0, dot(hdrBase, vec3<f32>(0.299, 0.587, 0.114)) - 0.95);
    let bloomColor = hdrBase * (bloomLuma * 0.38);
    var hdr = (hdrBase + bloomColor + godRays) * u.sunColor.w;

    // 5. Subtle Miniature Diorama Vignette
    let centered = in.uv - vec2<f32>(0.5, 0.5);
    let vig = 1.0 - dot(centered, centered) * 0.38;
    hdr *= vig;

    // 6. Filmic ACES Tone Mapping & Warm Color Grading
    var mapped = acesFilmic(hdr);
    let luma = dot(mapped, vec3<f32>(0.2126, 0.7152, 0.0722));
    mapped = clamp(mix(vec3<f32>(luma), mapped, 1.18), vec3<f32>(0.0), vec3<f32>(1.0));
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
    gridOrigin    : vec4<f32>,
    discoveryGrid : array<vec4<f32>, 16>,
    weatherParams : vec4<f32>,
    windDirection : vec4<f32>,
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
    let q = abs(in.localPos) - in.halfSize + vec2<f32>(in.radius, in.radius);
    let boxDist = length(max(q, vec2<f32>(0.0, 0.0))) + min(max(q.x, q.y), 0.0) - in.radius;
    let boxFw = max(fwidth(boxDist), 0.70);
    let lineDist = abs(in.localPos.x) - in.radius;
    let lineFw = max(fwidth(lineDist), 0.70);

    if (in.mode == 2u) {
        // Analytical Rounded Rectangle / Box with 4x Rotated-Grid Supersampled Subpixel Coverage (RGSS 4x)
        let s0 = clamp(0.5 - (boxDist + 0.375 * boxFw - 0.125 * boxFw) / boxFw, 0.0, 1.0);
        let s1 = clamp(0.5 - (boxDist - 0.125 * boxFw - 0.375 * boxFw) / boxFw, 0.0, 1.0);
        let s2 = clamp(0.5 - (boxDist + 0.125 * boxFw + 0.375 * boxFw) / boxFw, 0.0, 1.0);
        let s3 = clamp(0.5 - (boxDist - 0.375 * boxFw + 0.125 * boxFw) / boxFw, 0.0, 1.0);
        let alpha = (s0 + s1 + s2 + s3) * 0.25;
        if (alpha <= 0.001) {
            discard;
        }
        return vec4<f32>(in.color.rgb, in.color.a * alpha);
    } else if (in.mode == 6u) {
        // Analytical 4x Antialiased Vector Line Stroke (Eatsbits subpixel coverage)
        let s0 = clamp(0.5 - (lineDist + 0.375 * lineFw - 0.125 * lineFw) / lineFw, 0.0, 1.0);
        let s1 = clamp(0.5 - (lineDist - 0.125 * lineFw - 0.375 * lineFw) / lineFw, 0.0, 1.0);
        let s2 = clamp(0.5 - (lineDist + 0.125 * lineFw + 0.375 * lineFw) / lineFw, 0.0, 1.0);
        let s3 = clamp(0.5 - (lineDist - 0.375 * lineFw + 0.125 * lineFw) / lineFw, 0.0, 1.0);
        let alpha = (s0 + s1 + s2 + s3) * 0.25;
        if (alpha <= 0.001) {
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

    outU.gridOrigin[0] = static_cast<float>(world.activeChunkX());
    outU.gridOrigin[1] = static_cast<float>(world.activeChunkZ());
    outU.gridOrigin[2] = ChartWorld::kChunkSize;
    outU.gridOrigin[3] = world.voronoi().isDungeonMode() ? -static_cast<float>(ChartWorld::kChunkRadius)
                                                          : static_cast<float>(ChartWorld::kChunkRadius);

    world.fillDiscoveryGrid(world.activeChunkX(), world.activeChunkZ(), outU.discoveryGrid);

    const auto& wtr = world.weather();
    auto heroWeather = world.evaluateLocalWeather(world.hero().pos.x, world.hero().pos.z);
    outU.weatherParams[0] = wtr.wetness;
    outU.weatherParams[1] = wtr.snowCover;
    outU.weatherParams[2] = wtr.stormSurge;
    outU.weatherParams[3] = wtr.lightningFlash;

    outU.windDirection[0] = wtr.windDirection.x;
    outU.windDirection[1] = heroWeather.intensity; // Continuous precipitation intensity [0.0 - 1.0]
    outU.windDirection[2] = wtr.windDirection.z;
    outU.windDirection[3] = static_cast<float>(heroWeather.type);

    if (wtr.type == WeatherType::Overcast || wtr.type == WeatherType::Downpour ||
        wtr.type == WeatherType::Thunderstorm || wtr.type == WeatherType::Blizzard) {
        float dim = (wtr.type == WeatherType::Thunderstorm || wtr.type == WeatherType::Blizzard) ? 0.55f : 0.72f;
        outU.sunColor[0] *= dim;
        outU.sunColor[1] *= dim;
        outU.sunColor[2] *= dim;
        outU.sunDir[3] *= 1.35f;
    }
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
    WGPURenderPipeline weatherParticlePipeline{nullptr};
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
    uint32_t activeWeatherParticleCount{0};
    uint32_t lastUploadedStaticStandeeCount{0};

    bool init(GLFWwindow* optWindow, uint32_t width, uint32_t height, const InkAtlasData& atlas) {
        targetWidth = width;
        targetHeight = height;

#if defined(__EMSCRIPTEN__)
        int hasDevice = EM_ASM_INT({
            return (typeof Module !== 'undefined' && Module['preinitializedWebGPUDevice']) ? 1 : 0;
        });
        if (hasDevice) {
            device = emscripten_webgpu_get_device();
        }

        WGPUInstanceDescriptor instDesc{};
        instance = wgpuCreateInstance(&instDesc);

        if (optWindow) {
            WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc{};
            canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
            canvasDesc.selector = WGPUStringView{ "#canvas", WGPU_STRLEN };

            WGPUSurfaceDescriptor surfDesc{};
            surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvasDesc);
            surface = wgpuInstanceCreateSurface(instance, &surfDesc);
        }

        if (!device && instance) {
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

            if (adapter) {
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
            }
        }

        if (!device) {
            device = emscripten_webgpu_get_device();
        }
        if (!device) {
            std::cerr << "[ChartRenderer] Failed to acquire WebGPU device in browser environment.\n";
            return false;
        }

        queue = wgpuDeviceGetQueue(device);

        if (surface) {
            outputFormat = WGPUTextureFormat_BGRA8Unorm;
            configureSurface(width, height);
        } else {
            outputFormat = WGPUTextureFormat_RGBA8Unorm;
        }

        return createPipelinesAndResources(width, height, atlas);
#else
        WGPUInstanceDescriptor instDesc{};
        instance = wgpuCreateInstance(&instDesc);
        if (!instance) return false;

        if (optWindow) {
#if defined(_WIN32)
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
#endif
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
        std::string sceneWgsl = std::string(kSceneWgslPart1) + kSceneWgslPart2a + kSceneWgslPart2b + kSceneWgslPart2bb + kSceneWgslPart2c + kSceneWgslPart3 + kSceneWgslPart4;
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
        shEntries[1].visibility = WGPUShaderStage_Vertex;
        shEntries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        shEntries[2].binding = 2;
        shEntries[2].visibility = WGPUShaderStage_Vertex;
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
        bglEntries[1].binding = 1;
        bglEntries[1].visibility = WGPUShaderStage_Vertex;
        bglEntries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        bglEntries[2].binding = 2;
        bglEntries[2].visibility = WGPUShaderStage_Vertex;
        bglEntries[2].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        bglEntries[3].binding = 3;
        bglEntries[3].visibility = WGPUShaderStage_Fragment;
        bglEntries[3].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        bglEntries[4].binding = 4;
        bglEntries[4].visibility = WGPUShaderStage_Fragment;
        bglEntries[4].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
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

        // 1b. Camera-Frustum Bounded Weather Particles Pipeline (Rain, Snow, Dust)
        WGPUBlendState weatherBlend{};
        weatherBlend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
        weatherBlend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        weatherBlend.color.operation = WGPUBlendOperation_Add;
        weatherBlend.alpha.srcFactor = WGPUBlendFactor_One;
        weatherBlend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        weatherBlend.alpha.operation = WGPUBlendOperation_Add;

        WGPUColorTargetState weatherTarget{};
        weatherTarget.format = WGPUTextureFormat_RGBA16Float;
        weatherTarget.blend = &weatherBlend;
        weatherTarget.writeMask = WGPUColorWriteMask_All;

        WGPUDepthStencilState weatherDepth{};
        weatherDepth.format = WGPUTextureFormat_Depth32Float;
        weatherDepth.depthWriteEnabled = WGPUOptionalBool_False;
        weatherDepth.depthCompare = WGPUCompareFunction_LessEqual;

        WGPUFragmentState weatherFrag{};
        weatherFrag.module = sceneSM;
        weatherFrag.entryPoint = WGPUStringView{"fs_weather_particles", 20};
        weatherFrag.targetCount = 1;
        weatherFrag.targets = &weatherTarget;

        WGPURenderPipelineDescriptor weatherDesc{};
        weatherDesc.layout = scenePL;
        weatherDesc.vertex.module = sceneSM;
        weatherDesc.vertex.entryPoint = WGPUStringView{"vs_weather_particles", 20};
        weatherDesc.fragment = &weatherFrag;
        weatherDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        weatherDesc.primitive.cullMode = WGPUCullMode_None;
        weatherDesc.depthStencil = &weatherDepth;
        weatherDesc.multisample.count = 1;
        weatherDesc.multisample.mask = ~0u;
        weatherParticlePipeline = wgpuDeviceCreateRenderPipeline(device, &weatherDesc);

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
        bool spriteBufferReallocated = false;
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
            spriteBufferReallocated = true;
        }
        if (!spList.empty()) {
            uint32_t currentStaticCount = world.staticStandeeCount();
            if (rebuildStaticWorldVerts || spriteBufferReallocated || currentStaticCount != lastUploadedStaticStandeeCount) {
                wgpuQueueWriteBuffer(queue, spriteBuffer, 0, spList.data(), spList.size() * sizeof(StandeeInstance));
                lastUploadedStaticStandeeCount = currentStaticCount;
            } else {
                uint32_t staticCnt = std::min<uint32_t>(currentStaticCount, static_cast<uint32_t>(spList.size()));
                uint32_t dynCnt = static_cast<uint32_t>(spList.size()) - staticCnt;
                if (dynCnt > 0) {
                    wgpuQueueWriteBuffer(
                        queue,
                        spriteBuffer,
                        static_cast<uint64_t>(staticCnt) * sizeof(StandeeInstance),
                        spList.data() + staticCnt,
                        static_cast<size_t>(dynCnt) * sizeof(StandeeInstance)
                    );
                }
            }
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

        auto heroWeather = world.evaluateLocalWeather(world.hero().pos.x, world.hero().pos.z);
        uint32_t wType = static_cast<uint32_t>(heroWeather.type);
        if (wType >= 2u && heroWeather.intensity > 0.005f) {
            // Allocate full particle instance budget; dynamic density filtering is done per-instance in shader
            activeWeatherParticleCount = 1536u;
        } else {
            activeWeatherParticleCount = 0u;
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
        if (activeWeatherParticleCount > 0 && weatherParticlePipeline) {
            wgpuRenderPassEncoderSetPipeline(scenePass, weatherParticlePipeline);
            wgpuRenderPassEncoderDraw(scenePass, activeWeatherParticleCount * 6u, 1u, 0u, 0u);
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
        if (weatherParticlePipeline) wgpuRenderPipelineRelease(weatherParticlePipeline);
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
    static constexpr auto kCrcTable = []() constexpr {
        std::array<uint32_t, 256> table{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int b = 0; b < 8; ++b) {
                c = (c >> 1u) ^ (0xEDB88320u & (-(static_cast<int32_t>(c & 1u))));
            }
            table[i] = c;
        }
        return table;
    }();

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = kCrcTable[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8u);
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

struct ChartLoopContext {
    ChartWorld* world{nullptr};
    ChartScriptEngine* scriptEngine{nullptr};
    std::string activeScript;
    AudioFoleyEngine* foleyEngine{nullptr};
    GLFWwindow* window{nullptr};
    GpuChartContext* ctx{nullptr};
    std::filesystem::file_time_type lastScriptWriteTime{};
    double lastTime{0.0};
    bool wasSpace{false};
    bool wasLeftClick{false};
    bool wasRightClick{false};
    bool wasK{false};
    bool wasE{false};
    bool wasM{false};
    bool was1{false};
    bool was2{false};
    bool was3{false};
    bool was4{false};
    bool was5{false};
    bool was6{false};
    bool wasT{false};
    bool wasY{false};
    bool wasLeftBracket{false};
    bool wasRightBracket{false};
    bool atlasMode{false};
    float targetAtlasZoom{0.0f};
    float stepSoundCooldown{0.0f};
    float hotReloadPollTimer{0.0f};
    float touchMoveX{0.0f};
    float touchMoveZ{0.0f};
    bool touchSprintOrDodge{false};
    ChartRenderer::FrameUpdateCallback frameCallback{nullptr};
};

static ChartLoopContext* s_activeLoop = nullptr;

static bool switchChartScene(ChartLoopContext& l, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "[Eatscharted] Chart recipe not found: " << path << "\n";
        return false;
    }
    if (l.scriptEngine->executeFile(path)) {
        l.activeScript = path;
#if !defined(__EMSCRIPTEN__)
        try {
            l.lastScriptWriteTime = std::filesystem::last_write_time(path);
        } catch (...) {}
#endif
        l.world->rebuildWorldGeometry(l.world->hero().pos.x, l.world->hero().pos.z);
        l.world->updateSimulation(0.016f, l.ctx->targetWidth, l.ctx->targetHeight);
        l.ctx->uploadWorldAndDynamicEntities(*l.world, true);
        l.world->clearMeshDirty();
        std::cout << "[Eatscharted] Loaded chart recipe: " << path << "\n";
#if defined(__EMSCRIPTEN__)
        std::string jsCall = "if (window.onSceneLoaded) window.onSceneLoaded('" + path + "');";
        emscripten_run_script(jsCall.c_str());
#endif
        return true;
    }
    return false;
}

static void renderSingleFrame(ChartLoopContext& l) {
    glfwPollEvents();

    double now = glfwGetTime();
    float dt = static_cast<float>(now - l.lastTime);
    if (dt < 0.0001f) dt = 0.0001f;
    if (dt > 0.1f) dt = 0.1f;
    l.lastTime = now;

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(l.window, &fbW, &fbH);
    if (fbW > 0 && fbH > 0 && (static_cast<uint32_t>(fbW) != l.ctx->targetWidth || static_cast<uint32_t>(fbH) != l.ctx->targetHeight)) {
        l.ctx->configureSurface(static_cast<uint32_t>(fbW), static_cast<uint32_t>(fbH));
        l.ctx->reallocateViewportTargets(static_cast<uint32_t>(fbW), static_cast<uint32_t>(fbH));
    }

#if !defined(__EMSCRIPTEN__)
    // Live Eatscript hot-reload check (throttled to 2 Hz to avoid per-frame NTFS metadata syscalls)
    l.hotReloadPollTimer += dt;
    if (l.hotReloadPollTimer >= 0.5f) {
        l.hotReloadPollTimer = 0.0f;
        if (!l.activeScript.empty() && std::filesystem::exists(l.activeScript)) {
            auto curWrite = std::filesystem::last_write_time(l.activeScript);
            if (curWrite != l.lastScriptWriteTime) {
                l.lastScriptWriteTime = curWrite;
                if (l.scriptEngine->executeFile(l.activeScript)) {
                    l.world->rebuildWorldGeometry(l.world->hero().pos.x, l.world->hero().pos.z);
                    l.world->updateSimulation(dt, l.ctx->targetWidth, l.ctx->targetHeight);
                    l.ctx->uploadWorldAndDynamicEntities(*l.world, true);
                    l.world->clearMeshDirty();
                    std::cout << "[Eatscharted] Hot-reloaded '" << l.activeScript << "'\n";
                }
            }
        }
    }
#endif

    // Scene preset switching (1, 2, 3, 4)
    bool k1 = glfwGetKey(l.window, GLFW_KEY_1) == GLFW_PRESS;
    if (k1 && !l.was1) switchChartScene(l, "scenes/shattered_isles.eats");
    l.was1 = k1;

    bool k2 = glfwGetKey(l.window, GLFW_KEY_2) == GLFW_PRESS;
    if (k2 && !l.was2) switchChartScene(l, "scenes/perilous_shores.eats");
    l.was2 = k2;

    bool k3 = glfwGetKey(l.window, GLFW_KEY_3) == GLFW_PRESS;
    if (k3 && !l.was3) switchChartScene(l, "scenes/highland_kingdom.eats");
    l.was3 = k3;

    bool k4 = glfwGetKey(l.window, GLFW_KEY_4) == GLFW_PRESS;
    if (k4 && !l.was4) switchChartScene(l, "scenes/sunken_sanctum_dungeon.eats");
    l.was4 = k4;

    bool k5 = glfwGetKey(l.window, GLFW_KEY_5) == GLFW_PRESS;
    if (k5 && !l.was5) switchChartScene(l, "scenes/sunscorched_wastes.eats");
    l.was5 = k5;

    bool k6 = glfwGetKey(l.window, GLFW_KEY_6) == GLFW_PRESS;
    if (k6 && !l.was6) switchChartScene(l, "scenes/procedural_dungeon.eats");
    l.was6 = k6;

    // Cat Quest Tabletop Hero Movement (Keyboard + Mobile Analog Touch Stick)
    float mx = l.touchMoveX;
    float mz = l.touchMoveZ;
    if (glfwGetKey(l.window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(l.window, GLFW_KEY_UP) == GLFW_PRESS)    mz += 1.0f;
    if (glfwGetKey(l.window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(l.window, GLFW_KEY_DOWN) == GLFW_PRESS)  mz -= 1.0f;
    if (glfwGetKey(l.window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(l.window, GLFW_KEY_RIGHT) == GLFW_PRESS) mx += 1.0f;
    if (glfwGetKey(l.window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(l.window, GLFW_KEY_LEFT) == GLFW_PRESS)  mx -= 1.0f;
    bool dodgeKey = l.touchSprintOrDodge ||
                    (glfwGetKey(l.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(l.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    bool wasSailing = l.world->hero().isSailing;
    l.world->moveHero(mx, mz, dodgeKey, dt);
    if (l.world->hero().isSailing != wasSailing) {
        l.foleyEngine->triggerSound(FoleySound::OceanSplash, 0.9f);
    }

    l.stepSoundCooldown = std::max(0.0f, l.stepSoundCooldown - dt);
    if ((std::abs(mx) > 0.01f || std::abs(mz) > 0.01f) && l.stepSoundCooldown <= 0.0f) {
        l.foleyEngine->triggerSound(l.world->hero().isSailing ? FoleySound::OceanSplash : FoleySound::ParchmentStep, 0.35f);
        l.stepSoundCooldown = l.world->hero().isSailing ? 0.35f : 0.172f;
    }

    // Melee Slash (Space / J / Desktop Left Click)
    bool spDown = (glfwGetKey(l.window, GLFW_KEY_SPACE) == GLFW_PRESS || glfwGetKey(l.window, GLFW_KEY_J) == GLFW_PRESS);
#if !defined(__EMSCRIPTEN__)
    bool lcDown = (glfwGetMouseButton(l.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
#else
    bool lcDown = false;
#endif
    if ((spDown && !l.wasSpace) || (lcDown && !l.wasLeftClick)) {
        l.world->triggerMeleeSlash();
        l.foleyEngine->triggerSound(FoleySound::SwordSlash, 0.85f);
    }
    l.wasSpace = spDown;
    l.wasLeftClick = lcDown;

    // Arcane Nova Spell (K / Desktop Right Click)
    bool kDown  = (glfwGetKey(l.window, GLFW_KEY_K) == GLFW_PRESS);
#if !defined(__EMSCRIPTEN__)
    bool rcDown = (glfwGetMouseButton(l.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
#else
    bool rcDown = false;
#endif
    if ((kDown && !l.wasK) || (rcDown && !l.wasRightClick)) {
        l.world->triggerArcaneSpell();
        l.foleyEngine->triggerSound(FoleySound::ArcaneNova, 0.95f);
    }
    l.wasK = kDown;
    l.wasRightClick = rcDown;

    // Enter / Exit One-Page Dungeon (E / Enter)
    bool eDown = (glfwGetKey(l.window, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(l.window, GLFW_KEY_ENTER) == GLFW_PRESS);
    if (eDown && !l.wasE) {
        if (l.world->tryToggleNearbyDungeon()) {
            l.foleyEngine->triggerSound(FoleySound::DungeonPortal, 1.0f);
            l.world->updateSimulation(dt, l.ctx->targetWidth, l.ctx->targetHeight);
            l.ctx->uploadWorldAndDynamicEntities(*l.world, true);
            l.world->clearMeshDirty();
        }
    }
    l.wasE = eDown;

    // Zoom-to-Atlas Toggle (M / Z)
    bool mDown = (glfwGetKey(l.window, GLFW_KEY_M) == GLFW_PRESS || glfwGetKey(l.window, GLFW_KEY_Z) == GLFW_PRESS);
    if (mDown && !l.wasM) {
        l.atlasMode = !l.atlasMode;
        l.targetAtlasZoom = l.atlasMode ? 1.0f : 0.0f;
    }
    l.wasM = mDown;

    // Day/Night Cycle Toggle (T) and Hour Stepping ([ / ])
    bool tDown = (glfwGetKey(l.window, GLFW_KEY_T) == GLFW_PRESS);
    if (tDown && !l.wasT) {
        l.world->setUseDayNightCycle(!l.world->useDayNightCycle());
        std::cout << "[Eatscharted] Day/Night Cycle " << (l.world->useDayNightCycle() ? "ENABLED" : "PAUSED") << "\n";
    }
    l.wasT = tDown;

    // Weather Cycle Manual Step (Y)
    bool yDown = (glfwGetKey(l.window, GLFW_KEY_Y) == GLFW_PRESS);
    if (yDown && !l.wasY) {
        uint8_t nextW = (static_cast<uint8_t>(l.world->weather().type) + 1) % 8;
        l.world->setWeatherType(static_cast<WeatherType>(nextW), 1.0f);
        auto wtrInfo = l.world->evaluateLocalWeather(l.world->hero().pos.x, l.world->hero().pos.z);
        l.world->triggerLocaleDiscovery("WEATHER CHANGED", wtrInfo.description);
        std::cout << "[Eatscharted] Weather stepped to: " << wtrInfo.description << " (" << weatherTypeName(static_cast<WeatherType>(nextW)) << ")\n";
        if (nextW == static_cast<uint8_t>(WeatherType::Thunderstorm)) {
            l.foleyEngine->triggerSound(FoleySound::ThunderClap, 0.95f);
        } else if (nextW == static_cast<uint8_t>(WeatherType::Downpour) || nextW == static_cast<uint8_t>(WeatherType::Drizzle)) {
            l.foleyEngine->triggerSound(FoleySound::RainPatter, 0.85f);
        } else if (nextW == static_cast<uint8_t>(WeatherType::Blizzard) || nextW == static_cast<uint8_t>(WeatherType::Sandstorm)) {
            l.foleyEngine->triggerSound(FoleySound::WindGust, 0.90f);
        }
    }
    l.wasY = yDown;

    bool leftBrack = (glfwGetKey(l.window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS);
    if (leftBrack && !l.wasLeftBracket) {
        l.world->setTimeOfDay(l.world->timeOfDay() - (1.0f / 24.0f));
        std::cout << "[Eatscharted] Time of Day stepped to: " << l.world->timeOfDay() << " (" << AtmosphereConfig::timePeriodName(l.world->timeOfDay()) << ")\n";
    }
    l.wasLeftBracket = leftBrack;

    bool rightBrack = (glfwGetKey(l.window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS);
    if (rightBrack && !l.wasRightBracket) {
        l.world->setTimeOfDay(l.world->timeOfDay() + (1.0f / 24.0f));
        std::cout << "[Eatscharted] Time of Day stepped to: " << l.world->timeOfDay() << " (" << AtmosphereConfig::timePeriodName(l.world->timeOfDay()) << ")\n";
    }
    l.wasRightBracket = rightBrack;

    // Smoothly interpolate Cat Quest Tabletop Camera <-> High-Altitude Perilous Shores Atlas View
    auto& cam = l.world->camera();
    cam.atlasZoom = std::lerp(cam.atlasZoom, l.targetAtlasZoom, std::clamp(dt * 5.0f, 0.0f, 1.0f));

    float camHeight = std::lerp(28.0f, 82.0f, cam.atlasZoom);
    float camBackZ  = std::lerp(25.0f, 22.0f, cam.atlasZoom);
    Vec3f desiredTarget = l.world->hero().pos;
    Vec3f desiredPos    = {desiredTarget.x, desiredTarget.y + camHeight, desiredTarget.z - camBackZ};

    float camFollow = std::clamp(dt * 8.0f, 0.0f, 1.0f);
    cam.target.x   = std::lerp(cam.target.x,   desiredTarget.x, camFollow);
    cam.target.y   = std::lerp(cam.target.y,   desiredTarget.y, camFollow);
    cam.target.z   = std::lerp(cam.target.z,   desiredTarget.z, camFollow);
    cam.position.x = std::lerp(cam.position.x, desiredPos.x,    camFollow);
    cam.position.y = std::lerp(cam.position.y, desiredPos.y,    camFollow);
    cam.position.z = std::lerp(cam.position.z, desiredPos.z,    camFollow);
    cam.focusDistance = std::sqrt(camHeight * camHeight + camBackZ * camBackZ);

    bool callbackRebuild = false;
    if (l.frameCallback) {
        callbackRebuild = l.frameCallback(l.window, *l.world, dt);
    }

    l.world->updateSimulation(dt, l.ctx->targetWidth, l.ctx->targetHeight);
    bool rebuildStatic = l.world->needsMeshUpload() || callbackRebuild;
    l.ctx->uploadWorldAndDynamicEntities(*l.world, rebuildStatic);
    l.world->clearMeshDirty();

    WGPUSurfaceTexture surfTex{};
    wgpuSurfaceGetCurrentTexture(l.ctx->surface, &surfTex);
    if (surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return;
    }

    WGPUTextureView backbufferView = wgpuTextureCreateView(surfTex.texture, nullptr);

    SceneUniforms u{};
    fillSceneUniforms(*l.world, l.ctx->targetWidth, l.ctx->targetHeight, static_cast<float>(now), u);
    wgpuQueueWriteBuffer(l.ctx->queue, l.ctx->uniformBuffer, 0, &u, sizeof(SceneUniforms));

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(l.ctx->device, nullptr);
    l.ctx->recordFramePasses(encoder, backbufferView);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(l.ctx->queue, 1, &cmd);
#if !defined(__EMSCRIPTEN__)
    wgpuSurfacePresent(l.ctx->surface);
#else
    static bool s_firstFramePresented = false;
    if (!s_firstFramePresented) {
        s_firstFramePresented = true;
        emscripten_run_script("if (window.hideAppLoadingScreen) window.hideAppLoadingScreen();");
    }
#endif

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(backbufferView);
    wgpuTextureRelease(surfTex.texture);
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
    world.clearMeshDirty();

    std::filesystem::file_time_type lastScriptWriteTime{};
#if !defined(__EMSCRIPTEN__)
    if (!watchedScriptPath.empty() && std::filesystem::exists(watchedScriptPath)) {
        lastScriptWriteTime = std::filesystem::last_write_time(watchedScriptPath);
    }
#endif

    std::cout << "\n====================================================================\n"
              << "  Eatscharted: 2.5D Living Cartography (Perilous Shores x Cat Quest)\n"
              << "====================================================================\n"
              << "  W / A / S / D or Arrows : Move Hero / Sail Caravel Across Map\n"
              << "  Left Shift              : Cat Quest Dodge-Roll / Sprint\n"
              << "  Space / J / Left Click  : Melee Sword Slash\n"
              << "  K / Right Click         : Cast Arcane Nova (Map-Surface Telegraph)\n"
              << "  E / Enter               : Enter / Exit Watabou One-Page Dungeon\n"
              << "  T                       : Toggle Dynamic Day/Night Cycle\n"
              << "  Y                       : Step Weather (Rain, Storm, Blizzard, Clear...)\n"
              << "  [ / ]                   : Step Time of Day (-1 hr / +1 hr)\n"
              << "  M / Z or Scroll Wheel   : Zoom-to-Atlas Cartographic View\n"
              << "  1 / 2 / 3 / 4 / 5 / 6   : Switch Chart Preset / Dungeon\n"
              << "====================================================================\n\n";

    ChartLoopContext loop;
    loop.world = &world;
    loop.scriptEngine = &scriptEngine;
    loop.activeScript = watchedScriptPath;
    loop.foleyEngine = &foleyEngine;

    world.setSoundCallback([&foleyEngine](uint16_t soundId, float intensity) {
        foleyEngine.triggerSound(soundId, intensity);
    });
    loop.window = window;
    loop.ctx = &ctx;
    loop.lastScriptWriteTime = lastScriptWriteTime;
    loop.lastTime = glfwGetTime();
    loop.frameCallback = std::move(frameCallback);
    s_activeLoop = &loop;

    glfwSetWindowUserPointer(window, &loop);
    glfwSetScrollCallback(window, [](GLFWwindow* w, double, double yoffset) {
        auto* l = static_cast<ChartLoopContext*>(glfwGetWindowUserPointer(w));
        if (l) {
            l->targetAtlasZoom = std::clamp(l->targetAtlasZoom - static_cast<float>(yoffset) * 0.14f, 0.0f, 1.0f);
            l->atlasMode = (l->targetAtlasZoom >= 0.5f);
        }
    });

#if defined(__EMSCRIPTEN__)
    static ChartLoopContext s_webLoop;
    static GpuChartContext s_webCtx;
    s_webCtx = std::move(ctx);
    s_webLoop = loop;
    s_webLoop.ctx = &s_webCtx;
    s_activeLoop = &s_webLoop;
    glfwSetWindowUserPointer(window, &s_webLoop);

    emscripten_set_main_loop_arg([](void* arg) {
        auto* l = static_cast<ChartLoopContext*>(arg);
        if (l && l->window && !glfwWindowShouldClose(l->window)) {
            renderSingleFrame(*l);
        }
    }, &s_webLoop, 0, 1);
    return 0;
#else
    while (!glfwWindowShouldClose(window)) {
        renderSingleFrame(loop);
    }

    ctx.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
#endif
}

#if defined(__EMSCRIPTEN__)
extern "C" {
EMSCRIPTEN_KEEPALIVE int eatscharted_load_scene(const char* scenePath) {
    if (s_activeLoop && scenePath) {
        return switchChartScene(*s_activeLoop, std::string(scenePath)) ? 1 : 0;
    }
    return 0;
}
EMSCRIPTEN_KEEPALIVE void eatscharted_set_time_of_day(float t) {
    if (s_activeLoop && s_activeLoop->world) {
        s_activeLoop->world->setTimeOfDay(t);
    }
}
EMSCRIPTEN_KEEPALIVE float eatscharted_get_time_of_day() {
    if (s_activeLoop && s_activeLoop->world) {
        return s_activeLoop->world->timeOfDay();
    }
    return 0.36f;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_toggle_time_progression() {
    if (s_activeLoop && s_activeLoop->world) {
        bool next = !s_activeLoop->world->useDayNightCycle();
        s_activeLoop->world->setUseDayNightCycle(next);
        return next ? 1 : 0;
    }
    return 1;
}
EMSCRIPTEN_KEEPALIVE void eatscharted_set_touch_move(float moveX, float moveZ, int sprintOrDodge) {
    if (s_activeLoop) {
        s_activeLoop->touchMoveX = moveX;
        s_activeLoop->touchMoveZ = moveZ;
        s_activeLoop->touchSprintOrDodge = (sprintOrDodge != 0);
    }
}
EMSCRIPTEN_KEEPALIVE void eatscharted_trigger_slash() {
    if (s_activeLoop && s_activeLoop->world && s_activeLoop->foleyEngine) {
        s_activeLoop->world->triggerMeleeSlash();
        s_activeLoop->foleyEngine->triggerSound(FoleySound::SwordSlash, 0.85f);
    }
}
EMSCRIPTEN_KEEPALIVE void eatscharted_trigger_spell() {
    if (s_activeLoop && s_activeLoop->world && s_activeLoop->foleyEngine) {
        s_activeLoop->world->triggerArcaneSpell();
        s_activeLoop->foleyEngine->triggerSound(FoleySound::ArcaneNova, 0.95f);
    }
}
EMSCRIPTEN_KEEPALIVE int eatscharted_toggle_dungeon() {
    if (s_activeLoop && s_activeLoop->world && s_activeLoop->foleyEngine && s_activeLoop->ctx) {
        if (s_activeLoop->world->tryToggleNearbyDungeon()) {
            s_activeLoop->foleyEngine->triggerSound(FoleySound::DungeonPortal, 1.0f);
            s_activeLoop->world->updateSimulation(0.016f, s_activeLoop->ctx->targetWidth, s_activeLoop->ctx->targetHeight);
            s_activeLoop->ctx->uploadWorldAndDynamicEntities(*s_activeLoop->world, true);
            s_activeLoop->world->clearMeshDirty();
            return 1;
        }
    }
    return 0;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_toggle_atlas() {
    if (s_activeLoop) {
        s_activeLoop->atlasMode = !s_activeLoop->atlasMode;
        s_activeLoop->targetAtlasZoom = s_activeLoop->atlasMode ? 1.0f : 0.0f;
        return s_activeLoop->atlasMode ? 1 : 0;
    }
    return 0;
}
EMSCRIPTEN_KEEPALIVE void eatscharted_add_atlas_zoom(float deltaZoom) {
    if (s_activeLoop) {
        s_activeLoop->targetAtlasZoom = std::clamp(s_activeLoop->targetAtlasZoom + deltaZoom, 0.0f, 1.0f);
        s_activeLoop->atlasMode = (s_activeLoop->targetAtlasZoom >= 0.5f);
    }
}
EMSCRIPTEN_KEEPALIVE int eatscharted_get_hero_hp() {
    return (s_activeLoop && s_activeLoop->world) ? s_activeLoop->world->hero().hp : 120;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_get_hero_max_hp() {
    return (s_activeLoop && s_activeLoop->world) ? s_activeLoop->world->hero().maxHp : 120;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_get_hero_level() {
    return (s_activeLoop && s_activeLoop->world) ? s_activeLoop->world->hero().level : 1;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_get_hero_gold() {
    return (s_activeLoop && s_activeLoop->world) ? s_activeLoop->world->hero().gold : 50;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_get_charted_chunks() {
    return (s_activeLoop && s_activeLoop->world) ? static_cast<int>(s_activeLoop->world->discoveredChunkCount()) : 0;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_is_sailing() {
    return (s_activeLoop && s_activeLoop->world && s_activeLoop->world->hero().isSailing) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int eatscharted_is_dungeon_mode() {
    return (s_activeLoop && s_activeLoop->world && s_activeLoop->world->voronoi().isDungeonMode()) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE const char* eatscharted_get_region_name() {
    if (s_activeLoop && s_activeLoop->world) {
        return s_activeLoop->world->hero().activeRegionName.c_str();
    }
    return "PORT PAWVEN";
}
EMSCRIPTEN_KEEPALIVE const char* eatscharted_get_region_sub() {
    if (s_activeLoop && s_activeLoop->world) {
        return s_activeLoop->world->hero().activeRegionSub.c_str();
    }
    return "Royal Cartographer's Harbor";
}
EMSCRIPTEN_KEEPALIVE void eatscharted_trigger_locale_discovery(const char* name, const char* sub) {
    if (s_activeLoop && s_activeLoop->world) {
        s_activeLoop->world->triggerLocaleDiscovery(name ? name : "", sub ? sub : "");
    }
}
EMSCRIPTEN_KEEPALIVE int eatscharted_is_locale_banner_active() {
    return (s_activeLoop && s_activeLoop->world && s_activeLoop->world->isLocaleBannerActive()) ? 1 : 0;
}
}
#endif

} // namespace eatscharted
