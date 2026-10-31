#include "eatscharted/ink_canvas.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace eatscharted {

namespace {

constexpr int kS = static_cast<int>(InkLayer::kTileSize); // 64x64

// Antique Cartography & Cat Quest Palette
constexpr RGBA8 kInkDark     { 34,  25,  19, 255};
constexpr RGBA8 kInkMid      { 68,  52,  39, 255};
constexpr RGBA8 kPaperCream  {242, 230, 204, 255};
constexpr RGBA8 kPaperWarm   {226, 209, 176, 255};
constexpr RGBA8 kPaperShadow {186, 166, 134, 255};

[[nodiscard]] uint32_t hash2D(int x, int y, uint32_t seed) noexcept {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u +
                 seed * 1442695041u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

[[nodiscard]] float rand01(int x, int y, uint32_t seed) noexcept {
    return static_cast<float>(hash2D(x, y, seed) & 0xFFFFu) / 65535.0f;
}

struct LayerSlice {
    RGBA8 albedo[kS * kS]{};
    float height[kS * kS]{};
    float emissive[kS * kS]{};
    Vec3f explicitNorm[kS * kS]{};
    bool hasExplicitNorm[kS * kS]{};

    void clear(RGBA8 c = {0, 0, 0, 0}, float h = 0.0f) noexcept {
        for (int i = 0; i < kS * kS; ++i) {
            albedo[i] = c;
            height[i] = h;
            emissive[i] = 0.0f;
            explicitNorm[i] = {0.0f, 0.0f, 1.0f};
            hasExplicitNorm[i] = false;
        }
    }

    void setPixel(int x, int y, RGBA8 c, float h = 0.5f, float em = 0.0f) noexcept {
        if (x < 0 || x >= kS || y < 0 || y >= kS) return;
        int idx = y * kS + x;
        albedo[idx] = c;
        height[idx] = h;
        emissive[idx] = em;
    }

    void setPixelNormal(int x, int y, RGBA8 c, Vec3f norm, float h = 0.5f, float em = 0.0f) noexcept {
        if (x < 0 || x >= kS || y < 0 || y >= kS) return;
        int idx = y * kS + x;
        albedo[idx] = c;
        height[idx] = h;
        emissive[idx] = em;
        float len = std::sqrt(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
        if (len > 1e-5f) { norm.x /= len; norm.y /= len; norm.z /= len; }
        explicitNorm[idx] = norm;
        hasExplicitNorm[idx] = true;
    }

    [[nodiscard]] RGBA8 getPixel(int x, int y) const noexcept {
        x = std::clamp(x, 0, kS - 1);
        y = std::clamp(y, 0, kS - 1);
        return albedo[y * kS + x];
    }

    void fillCircle(float cx, float cy, float r, RGBA8 fillCol, float h = 0.6f, float em = 0.0f) noexcept {
        int x0 = std::max(0, static_cast<int>(cx - r - 1));
        int x1 = std::min(kS - 1, static_cast<int>(cx + r + 1));
        int y0 = std::max(0, static_cast<int>(cy - r - 1));
        int y1 = std::min(kS - 1, static_cast<int>(cy + r + 1));
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                float dx = (static_cast<float>(x) + 0.5f - cx) / r;
                float dy = (static_cast<float>(y) + 0.5f - cy) / r;
                float d2 = dx * dx + dy * dy;
                if (d2 <= 1.0f) {
                    float nz = std::sqrt(std::max(0.05f, 1.0f - d2));
                    float domeH = h + nz * 0.35f;
                    setPixelNormal(x, y, fillCol, {dx * 0.85f, -dy * 0.85f, nz}, domeH, em);
                }
            }
        }
    }

    void fillRect(int x0, int y0, int x1, int y1, RGBA8 col, float h = 0.5f, float em = 0.0f) noexcept {
        for (int y = std::max(0, y0); y <= std::min(kS - 1, y1); ++y) {
            for (int x = std::max(0, x0); x <= std::min(kS - 1, x1); ++x) {
                setPixel(x, y, col, h, em);
            }
        }
    }

    void drawLine(float x0, float y0, float x1, float y1, float radius, RGBA8 col, float h = 0.3f, float em = 0.0f) noexcept {
        float dx = x1 - x0;
        float dy = y1 - y0;
        int steps = std::max(1, static_cast<int>(std::sqrt(dx * dx + dy * dy) * 2.0f));
        for (int i = 0; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            fillCircle(x0 + dx * t, y0 + dy * t, radius, col, h, em);
        }
    }

    void blendPixel(int x, int y, RGBA8 c, Vec3f norm = {0.0f, 0.0f, 1.0f}, float h = 0.5f, float em = 0.0f) noexcept {
        if (x < 0 || x >= kS || y < 0 || y >= kS) return;
        if (c.a == 0) return;
        int idx = y * kS + x;
        float srcA = static_cast<float>(c.a) / 255.0f;
        float dstA = static_cast<float>(albedo[idx].a) / 255.0f;
        float outA = srcA + dstA * (1.0f - srcA);
        if (outA > 1e-4f) {
            float r = (static_cast<float>(c.r) * srcA + static_cast<float>(albedo[idx].r) * dstA * (1.0f - srcA)) / outA;
            float g = (static_cast<float>(c.g) * srcA + static_cast<float>(albedo[idx].g) * dstA * (1.0f - srcA)) / outA;
            float b = (static_cast<float>(c.b) * srcA + static_cast<float>(albedo[idx].b) * dstA * (1.0f - srcA)) / outA;
            albedo[idx] = RGBA8{
                static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(outA * 255.0f, 0.0f, 255.0f))
            };
        }
        height[idx] = std::max(height[idx], h);
        emissive[idx] = std::max(emissive[idx], em);
        if (norm.x != 0.0f || norm.y != 0.0f || norm.z != 1.0f) {
            explicitNorm[idx] = norm;
            hasExplicitNorm[idx] = true;
        }
    }

    void drawInkDot(float cx, float cy, float radius, RGBA8 col, float h = 0.4f) noexcept {
        int ix0 = std::max(0, static_cast<int>(cx - radius - 1.0f));
        int ix1 = std::min(kS - 1, static_cast<int>(cx + radius + 1.0f));
        int iy0 = std::max(0, static_cast<int>(cy - radius - 1.0f));
        int iy1 = std::min(kS - 1, static_cast<int>(cy + radius + 1.0f));
        for (int y = iy0; y <= iy1; ++y) {
            for (int x = ix0; x <= ix1; ++x) {
                float dist = std::hypot(static_cast<float>(x) + 0.5f - cx, static_cast<float>(y) + 0.5f - cy);
                if (dist <= radius) {
                    float cov = std::clamp(radius - dist + 0.5f, 0.0f, 1.0f);
                    RGBA8 c = col;
                    c.a = static_cast<uint8_t>(std::round(static_cast<float>(col.a) * cov));
                    blendPixel(x, y, c, {0.0f, -0.2f, 0.98f}, h);
                }
            }
        }
    }

    void drawInkLine(float x0, float y0, float x1, float y1, float radius, RGBA8 col, float h = 0.4f) noexcept {
        float dx = x1 - x0;
        float dy = y1 - y0;
        float len = std::sqrt(dx * dx + dy * dy);
        int steps = std::max(1, static_cast<int>(len * 2.5f));
        for (int i = 0; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            drawInkDot(x0 + dx * t, y0 + dy * t, radius, col, h);
        }
    }

    void drawInkBezier(float x0, float y0, float cx, float cy, float x1, float y1,
                       float r0, float r1, RGBA8 col, float h = 0.4f) noexcept {
        float d0x = cx - x0, d0y = cy - y0;
        float d1x = x1 - cx, d1y = y1 - cy;
        float estLen = std::sqrt(d0x * d0x + d0y * d0y) + std::sqrt(d1x * d1x + d1y * d1y);
        int steps = std::max(3, static_cast<int>(estLen * 2.5f));
        for (int i = 0; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float it = 1.0f - t;
            float px = it * it * x0 + 2.0f * it * t * cx + t * t * x1;
            float py = it * it * y0 + 2.0f * it * t * cy + t * t * y1;
            float r = r0 + (r1 - r0) * t;
            drawInkDot(px, py, r, col, h);
        }
    }

    // Adds a crisp hand-inked outline + subtle warm paper standee rim around opaque pixels
    void applyInkOutlineAndPaperCutout() noexcept {
        std::vector<RGBA8> copy(albedo, albedo + (kS * kS));

        // Pass 1: 1px dark India-ink outline around any opaque shape
        for (int y = 1; y < kS - 1; ++y) {
            for (int x = 1; x < kS - 1; ++x) {
                if (copy[y * kS + x].a > 128) continue;
                bool touchesOpaque = false;
                for (int dy = -1; dy <= 1 && !touchesOpaque; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (copy[(y + dy) * kS + (x + dx)].a > 128) {
                            touchesOpaque = true;
                            break;
                        }
                    }
                }
                if (touchesOpaque) {
                    setPixel(x, y, kInkDark, 0.25f, 0.0f);
                }
            }
        }
    }

    void bakeNormalsToAtlas(uint32_t layerIdx, InkAtlasData& out) const noexcept {
        size_t sliceOffset = static_cast<size_t>(layerIdx) * kS * kS * 4;
        for (int y = 0; y < kS; ++y) {
            for (int x = 0; x < kS; ++x) {
                int idx = y * kS + x;
                size_t dst = sliceOffset + static_cast<size_t>(idx) * 4;
                out.albedoBytes[dst + 0] = albedo[idx].r;
                out.albedoBytes[dst + 1] = albedo[idx].g;
                out.albedoBytes[dst + 2] = albedo[idx].b;
                out.albedoBytes[dst + 3] = albedo[idx].a;

                float nx = 0.0f, ny = 0.0f, nz = 1.0f;
                if (hasExplicitNorm[idx]) {
                    nx = explicitNorm[idx].x;
                    ny = explicitNorm[idx].y;
                    nz = explicitNorm[idx].z;
                } else {
                    auto hAt = [&](int sx, int sy) -> float {
                        sx = std::clamp(sx, 0, kS - 1);
                        sy = std::clamp(sy, 0, kS - 1);
                        return height[sy * kS + sx];
                    };
                    float dX = (hAt(x + 1, y) - hAt(x - 1, y)) * 2.2f;
                    float dY = (hAt(x, y + 1) - hAt(x, y - 1)) * 2.2f;
                    nx = -dX;
                    ny = dY; // +Y is up in world space (whereas +y in image is down)
                    nz = 1.0f;
                    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (len > 1e-5f) { nx /= len; ny /= len; nz /= len; }
                }

                out.normalBytes[dst + 0] = static_cast<uint8_t>(std::clamp((nx * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                out.normalBytes[dst + 1] = static_cast<uint8_t>(std::clamp((ny * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                out.normalBytes[dst + 2] = static_cast<uint8_t>(std::clamp((nz * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                out.normalBytes[dst + 3] = static_cast<uint8_t>(std::clamp(emissive[idx] * 255.0f, 0.0f, 255.0f));
            }
        }
    }
};

// Realms-style 2.5D Faceted Rock Formation & Sea Stack Generator
// Synthesizes crystalline/columnar basalt & granite rock pillars with per-facet 3D normals,
// wet dark shoreline base + seafoam fringe (for sea stacks), and top-ledge moss/highlights.
void synthRockFormationStandee(LayerSlice& s, uint32_t seed, int style) {
    // style 0: Tall Craggy Rock Pillar / Spire
    // style 1: Broad Clustered Boulder Outcrop
    // style 2: Offshore Jagged Sea Stack with Wet Basalt & Seafoam Base
    s.clear();

    struct RockColumn {
        float cx;
        float topY;
        float botY;
        float topHalfW;
        float botHalfW;
        float ridgeOffset;
    };

    RockColumn cols[4]{};
    int colCount = 3;
    if (style == 0) {
        cols[0] = {21.0f, 22.0f, 60.0f, 3.5f, 11.0f, -1.5f};
        cols[1] = {33.0f,  7.0f, 61.0f, 4.5f, 15.0f,  1.0f};
        cols[2] = {44.0f, 26.0f, 60.0f, 3.0f, 10.5f,  1.8f};
        colCount = 3;
    } else if (style == 1) {
        cols[0] = {18.0f, 30.0f, 60.0f, 5.5f, 13.0f, -1.2f};
        cols[1] = {32.0f, 20.0f, 61.0f, 7.0f, 16.5f,  0.5f};
        cols[2] = {46.0f, 28.0f, 60.0f, 5.0f, 12.5f,  1.5f};
        colCount = 3;
    } else {
        // Sea Stack: Dramatic towering twin sea spires + low reef ledge
        cols[0] = {20.0f, 18.0f, 59.0f, 3.0f, 11.5f, -1.0f};
        cols[1] = {34.0f,  6.0f, 60.0f, 4.0f, 14.5f,  0.8f};
        cols[2] = {46.0f, 32.0f, 59.0f, 3.5f,  9.5f,  1.4f};
        colCount = 3;
    }

    for (int ci = 0; ci < colCount; ++ci) {
        const auto& c = cols[ci];
        for (int y = static_cast<int>(c.topY); y <= static_cast<int>(c.botY); ++y) {
            float t = ( static_cast<float>(y) - c.topY ) / std::max(1.0f, c.botY - c.topY);
            // Jagged stepped cliff profile
            float stepJag = std::sin(y * 0.42f + static_cast<float>(ci * 3 + (seed & 7u))) * 1.4f +
                            std::cos(y * 0.85f + static_cast<float>(ci)) * 0.7f;
            float hw = c.topHalfW + t * (c.botHalfW - c.topHalfW) + stepJag;
            int leftX  = static_cast<int>(std::round(c.cx - hw));
            int rightX = static_cast<int>(std::round(c.cx + hw));
            float ridgeX = c.cx + c.ridgeOffset + std::sin(y * 0.31f + static_cast<float>(ci * 5)) * 2.2f;

            for (int x = leftX; x <= rightX; ++x) {
                float nNoise = rand01(x, y, seed + static_cast<uint32_t>(ci * 19));
                // Horizontal sedimentary/basalt fracture strata
                float strata = std::sin(static_cast<float>(y) * 0.65f + static_cast<float>(x) * 0.12f);
                bool isCrevice = (strata > 0.82f);

                // Compute 3D planar facet normal for crisp 2.5D lighting
                float facetNx = (static_cast<float>(x) < ridgeX) ? -0.62f : 0.62f;
                if (std::abs(static_cast<float>(x) - ridgeX) < 1.8f) {
                    facetNx = 0.0f; // Central front-facing arête
                }
                float facetNy = 0.25f + (1.0f - t) * 0.35f + (strata * 0.12f);
                float facetNz = 0.72f;

                // Base stone coloration (warm sunlit granite/slate on left, cool deep slate-basalt on right)
                float baseTone = (static_cast<float>(x) < ridgeX) ? 0.76f : 0.48f;
                if (std::abs(static_cast<float>(x) - ridgeX) < 1.3f) {
                    baseTone = 0.86f; // Crisp highlighted ridge arête
                }
                if (isCrevice) {
                    baseTone *= 0.62f;
                    facetNy = -0.35f;
                }
                // Ambient occlusion toward the base and between columns
                float ao = 1.0f - 0.32f * (t * t);
                baseTone *= ao * (0.92f + 0.14f * nNoise);

                uint8_t r = static_cast<uint8_t>(std::clamp(baseTone * 168.0f + 18.0f, 20.0f, 245.0f));
                uint8_t g = static_cast<uint8_t>(std::clamp(baseTone * 174.0f + 20.0f, 22.0f, 248.0f));
                uint8_t b = static_cast<uint8_t>(std::clamp(baseTone * 186.0f + 26.0f, 28.0f, 252.0f));

                // Lush emerald moss / coastal lichen cap on upper sun-facing ledges
                if (t < 0.28f && !isCrevice && ((x + y) % 2 == 0 || t < 0.14f)) {
                    r = static_cast<uint8_t>(54.0f + nNoise * 28.0f);
                    g = static_cast<uint8_t>(138.0f + nNoise * 42.0f);
                    b = static_cast<uint8_t>(58.0f + nNoise * 22.0f);
                    facetNy = 0.82f;
                }

                // Sea Stack wet dark basalt waterline & seafoam splash ring
                if (style == 2) {
                    if (y >= 52 && y <= 57) {
                        r = static_cast<uint8_t>(r * 0.48f);
                        g = static_cast<uint8_t>(g * 0.52f);
                        b = static_cast<uint8_t>(b * 0.58f);
                    } else if (y > 57) {
                        // Crisp white-aqua seafoam ring lapping the sea stack base
                        r = static_cast<uint8_t>(210.0f + nNoise * 40.0f);
                        g = static_cast<uint8_t>(245.0f + nNoise * 10.0f);
                        b = static_cast<uint8_t>(242.0f + nNoise * 12.0f);
                        facetNx = 0.0f;
                        facetNy = 0.92f;
                        facetNz = 0.38f;
                    }
                }

                float hVal = 0.55f + (1.0f - std::abs(static_cast<float>(x) - ridgeX) / std::max(1.0f, hw)) * 0.40f;
                s.setPixelNormal(x, y, RGBA8{r, g, b, 255}, {facetNx, facetNy, facetNz}, hVal);
            }
        }
    }
}

// Sorcery! x Watabou Hand-Drawn Cartographic Foliage & Landscape Flourish Synthesizer
// Replaces chunky pixel-art clusters with elegant scalloped pen-and-ink contours,
// soft watercolor washes, hand-inked underside hachures, and delicate parchment stippling.
void synthForestCanopy(LayerSlice& s, uint32_t seed, int style) {
    s.clear();
    // style 0: Sorcery! Hand-Inked Deciduous Oak (Scalloped Pen Lobes & Watercolor Wash)
    // style 1: Sorcery! / Watabou Tiered Boreal Pine Spire
    // style 2: Hand-Drawn Coastal Palm & Quill Fronds
    // style 3: Minimalist Pen-and-Ink Cartographic Grass Tufts & Wildflowers

    auto drawScallopedCanopyLobe = [&](float cx, float cy, float rx, float ry,
                                       RGBA8 colLit, RGBA8 colMid, RGBA8 colShade,
                                       float zBias, uint32_t lobeSeed) {
        int x0 = std::max(0, static_cast<int>(cx - rx - 2.0f));
        int x1 = std::min(kS - 1, static_cast<int>(cx + rx + 2.0f));
        int y0 = std::max(0, static_cast<int>(cy - ry - 2.0f));
        int y1 = std::min(kS - 1, static_cast<int>(cy + ry + 2.0f));

        // 1. Soft watercolor dome wash
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                float dx = (static_cast<float>(x) + 0.5f - cx) / rx;
                float dy = (static_cast<float>(y) + 0.5f - cy) / ry;
                float d2 = dx * dx + dy * dy;
                if (d2 <= 1.0f) {
                    float d = std::sqrt(d2);
                    float nz = std::sqrt(std::max(0.04f, 1.0f - d2));
                    // Directional lighting: sun from upper-left (-X, -Y)
                    float shade = (dy * 0.65f + dx * 0.35f);
                    RGBA8 c = colMid;
                    if (shade < -0.12f) {
                        float t = std::clamp((-shade - 0.12f) / 0.65f, 0.0f, 1.0f);
                        c.r = static_cast<uint8_t>(colMid.r + (colLit.r - colMid.r) * t);
                        c.g = static_cast<uint8_t>(colMid.g + (colLit.g - colMid.g) * t);
                        c.b = static_cast<uint8_t>(colMid.b + (colLit.b - colMid.b) * t);
                    } else if (shade > 0.08f) {
                        float t = std::clamp((shade - 0.08f) / 0.65f, 0.0f, 1.0f);
                        c.r = static_cast<uint8_t>(colMid.r + (colShade.r - colMid.r) * t);
                        c.g = static_cast<uint8_t>(colMid.g + (colShade.g - colMid.g) * t);
                        c.b = static_cast<uint8_t>(colMid.b + (colShade.b - colMid.b) * t);
                    }
                    // Watercolor pigment pooling near lobe boundary
                    if (d > 0.82f) {
                        float pool = (d - 0.82f) / 0.18f;
                        c.r = static_cast<uint8_t>(c.r * (1.0f - pool * 0.22f));
                        c.g = static_cast<uint8_t>(c.g * (1.0f - pool * 0.22f));
                        c.b = static_cast<uint8_t>(c.b * (1.0f - pool * 0.22f));
                    }
                    // Smooth boundary antialiasing
                    if (d > 0.90f) {
                        float alphaMul = std::clamp((1.0f - d) / 0.10f, 0.0f, 1.0f);
                        c.a = static_cast<uint8_t>(255.0f * alphaMul);
                    }
                    Vec3f norm{dx * 0.75f, -dy * 0.75f, nz};
                    s.blendPixel(x, y, c, norm, zBias + nz * 0.25f);
                }
            }
        }

        // 2. Hand-drawn pen ink hatching on shaded lower-right crescent (Sorcery! style)
        int numHatches = 4 + static_cast<int>(rx * 0.25f);
        float baseHatchAngle = 0.45f + static_cast<float>(lobeSeed & 3u) * 0.04f;
        for (int h = 0; h < numHatches; ++h) {
            float frac = static_cast<float>(h + 1) / static_cast<float>(numHatches + 2);
            float hAng = baseHatchAngle + frac * 1.55f; // Radian angle in lower-right sector
            float hR = 0.55f + 0.32f * (frac * frac);
            float hx = cx + rx * hR * std::cos(hAng);
            float hy = cy + ry * hR * std::sin(hAng);
            // Short 45-degree ink hatch stroke
            float hLen = 2.0f + 1.2f * std::sin(frac * 3.14159f);
            s.drawInkLine(hx - hLen * 0.7f, hy - hLen * 0.7f,
                          hx + hLen * 0.7f, hy + hLen * 0.7f,
                          0.55f, kInkDark, zBias + 0.10f);
        }

        // 3. Delicate ink stipple dots in the shadow pocket
        for (int st = 0; st < 3; ++st) {
            float sAng = 0.7f + static_cast<float>(st) * 0.45f;
            float sR = 0.42f + static_cast<float>(st) * 0.18f;
            s.drawInkDot(cx + rx * sR * std::cos(sAng),
                         cy + ry * sR * std::sin(sAng),
                         0.55f, kInkDark, zBias + 0.12f);
        }

        // 4. Hand-drawn ink pen outline tracing the lobe perimeter (toned to fine quill nib)
        int arcSteps = static_cast<int>((rx + ry) * 3.14159f * 0.75f);
        float prevX = cx + rx;
        float prevY = cy;
        for (int i = 1; i <= arcSteps; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(arcSteps) * 6.2831853f;
            float px = cx + rx * std::cos(a);
            float py = cy + ry * std::sin(a);
            s.drawInkLine(prevX, prevY, px, py, 0.38f, kInkDark, zBias + 0.15f);
            prevX = px;
            prevY = py;
        }
    };

    if (style == 0) {
        // Style 0: Sorcery! Hand-Inked Deciduous Oak (Scalloped Pen Lobes & Watercolor Wash)
        // 1. Hand-inked gnarled trunk & root flares
        constexpr RGBA8 kBarkLit  {138, 102,  72, 255};
        constexpr RGBA8 kBarkMid  {102,  72,  48, 255};
        constexpr RGBA8 kBarkShade{ 68,  44,  28, 255};

        for (int y = 36; y <= 60; ++y) {
            float t = static_cast<float>(y - 36) / 24.0f;
            float flare = t * t * 4.8f;
            int hw = static_cast<int>(3.0f + flare);
            for (int x = 32 - hw; x <= 32 + hw; ++x) {
                float dx = static_cast<float>(x - 32) / std::max(1.0f, static_cast<float>(hw));
                float shade = (dx * 0.45f + 0.5f);
                RGBA8 c = kBarkMid;
                if (shade < 0.35f) c = kBarkLit;
                else if (shade > 0.65f) c = kBarkShade;
                s.blendPixel(x, y, c, {dx * 0.7f, -0.1f, 0.7f}, 0.45f);
            }
        }

        // Hand-drawn pen ink bark contour & root fissures
        s.drawInkBezier(32.0f, 36.0f, 31.0f, 48.0f, 26.0f, 60.0f, 0.8f, 1.2f, kInkDark, 0.48f); // Left root flare
        s.drawInkBezier(33.0f, 36.0f, 34.0f, 48.0f, 38.0f, 60.0f, 0.8f, 1.2f, kInkDark, 0.48f); // Right root flare
        s.drawInkBezier(32.0f, 37.0f, 32.5f, 46.0f, 32.0f, 57.0f, 0.6f, 0.7f, kInkDark, 0.48f); // Center bark fissure
        for (int y = 42; y <= 54; y += 4) {
            s.drawInkLine(30.0f, static_cast<float>(y), 34.5f, static_cast<float>(y + 1), 0.55f, kInkDark, 0.49f);
        }

        // 2. Multi-tiered scalloped canopy lobes with watercolor wash
        constexpr RGBA8 kOakDeep  { 28,  68,  30, 255};
        constexpr RGBA8 kOakShade { 46, 102,  38, 255};
        constexpr RGBA8 kOakMid   { 72, 148,  48, 255};
        constexpr RGBA8 kOakLit   {122, 192,  56, 255};
        constexpr RGBA8 kOakSun   {156, 218,  72, 255};

        // Tier 1: Background shadow lobes
        drawScallopedCanopyLobe(20.0f, 38.0f, 11.5f,  9.5f, kOakMid, kOakShade, kOakDeep, 0.50f, seed + 1u);
        drawScallopedCanopyLobe(44.0f, 38.0f, 11.5f,  9.5f, kOakMid, kOakShade, kOakDeep, 0.50f, seed + 2u);
        drawScallopedCanopyLobe(32.0f, 40.0f, 12.5f,  9.0f, kOakMid, kOakShade, kOakDeep, 0.52f, seed + 3u);

        // Tier 2: Mid-canopy voluminous lobes
        drawScallopedCanopyLobe(18.0f, 27.0f, 12.0f, 10.5f, kOakLit, kOakMid, kOakShade, 0.64f, seed + 4u);
        drawScallopedCanopyLobe(46.0f, 27.0f, 12.0f, 10.5f, kOakLit, kOakMid, kOakShade, 0.64f, seed + 5u);
        drawScallopedCanopyLobe(32.0f, 28.0f, 14.0f, 11.5f, kOakLit, kOakMid, kOakShade, 0.70f, seed + 6u);

        // Tier 3: Sun-drenched crown lobes
        drawScallopedCanopyLobe(24.0f, 17.0f, 11.0f,  9.5f, kOakSun, kOakLit, kOakMid, 0.80f, seed + 7u);
        drawScallopedCanopyLobe(40.0f, 17.0f, 11.0f,  9.5f, kOakSun, kOakLit, kOakMid, 0.80f, seed + 8u);
        drawScallopedCanopyLobe(32.0f, 13.0f, 12.0f,  9.5f, kOakSun, kOakLit, kOakMid, 0.86f, seed + 9u);

        // Base ground grass tufts flanking the roots
        s.drawInkBezier(25.0f, 60.0f, 23.0f, 57.0f, 20.0f, 56.0f, 0.9f, 0.4f, kOakDeep, 0.42f);
        s.drawInkBezier(39.0f, 60.0f, 41.0f, 57.0f, 44.0f, 56.0f, 0.9f, 0.4f, kOakDeep, 0.42f);

    } else if (style == 1) {
        // Style 1: Sorcery! / Watabou Hand-Inked Boreal Pine Spire
        constexpr RGBA8 kBarkMid  {102,  72,  48, 255};
        constexpr RGBA8 kPineDeep { 18,  52,  36, 255};
        constexpr RGBA8 kPineShade{ 28,  82,  52, 255};
        constexpr RGBA8 kPineMid  { 48, 126,  72, 255};
        constexpr RGBA8 kPineLit  { 88, 172,  90, 255};

        // Slender timber trunk
        s.drawInkBezier(32.0f, 24.0f, 32.0f, 44.0f, 32.0f, 60.0f, 1.4f, 1.8f, kBarkMid, 0.45f);
        s.drawInkBezier(31.2f, 24.0f, 31.2f, 44.0f, 30.5f, 60.0f, 0.6f, 0.8f, kInkDark, 0.48f);
        s.drawInkBezier(32.8f, 24.0f, 32.8f, 44.0f, 33.5f, 60.0f, 0.6f, 0.8f, kInkDark, 0.48f);

        auto drawPineTier = [&](float cx, float topY, float botY, float hw, float zBias) {
            int y0 = static_cast<int>(topY);
            int y1 = static_cast<int>(botY);
            for (int y = y0; y <= y1; ++y) {
                float t = static_cast<float>(y - y0) / std::max(1.0f, botY - topY);
                float curHw = t * hw * (0.85f + 0.15f * std::sin(t * 3.14159f));
                for (float dx = -curHw; dx <= curHw; dx += 0.7f) {
                    float normX = dx / std::max(1.0f, curHw);
                    RGBA8 c = kPineMid;
                    if (normX < -0.2f && t < 0.6f) c = kPineLit;
                    else if (normX > 0.2f || t > 0.75f) c = kPineShade;
                    if (t > 0.88f) c = kPineDeep;
                    Vec3f norm{normX * 0.7f, 0.3f - t * 0.4f, 0.8f};
                    s.blendPixel(static_cast<int>(cx + dx), y, c, norm, zBias + (1.0f - t) * 0.2f);
                }
            }
            // Hand-inked drooping bough outlines & needle hatching
            s.drawInkBezier(cx, topY, cx - hw * 0.5f, (topY + botY) * 0.5f, cx - hw, botY, 0.7f, 0.8f, kInkDark, zBias + 0.15f);
            s.drawInkBezier(cx, topY, cx + hw * 0.5f, (topY + botY) * 0.5f, cx + hw, botY, 0.7f, 0.8f, kInkDark, zBias + 0.15f);
            s.drawInkBezier(cx - hw, botY, cx, botY - 2.0f, cx + hw, botY, 0.7f, 0.7f, kInkDark, zBias + 0.15f);
            for (float hx = 2.0f; hx < hw * 0.9f; hx += 3.0f) {
                s.drawInkLine(cx + hx, botY - 3.0f, cx + hx + 1.5f, botY - 0.5f, 0.55f, kInkDark, zBias + 0.18f);
            }
        };

        drawPineTier(32.0f, 40.0f, 57.0f, 17.0f, 0.52f);
        drawPineTier(32.0f, 27.0f, 43.0f, 14.0f, 0.62f);
        drawPineTier(32.0f, 15.0f, 29.0f, 10.5f, 0.72f);
        drawPineTier(32.0f,  5.0f, 17.0f,  6.5f, 0.82f);

    } else if (style == 2) {
        // Style 2: Sorcery! Hand-Inked Coastal Date Palm
        constexpr RGBA8 kBarkMid     {118,  86,  58, 255};
        constexpr RGBA8 kPalmFrondLit {142, 212,  72, 255};
        constexpr RGBA8 kPalmFrondMid { 78, 168,  54, 255};
        constexpr RGBA8 kPalmFrondDeep{ 34,  96,  38, 255};

        // Gracefully arched palm trunk
        s.drawInkBezier(28.0f, 60.0f, 31.0f, 42.0f, 35.0f, 25.0f, 2.0f, 1.2f, kBarkMid, 0.45f);
        s.drawInkBezier(27.0f, 60.0f, 30.0f, 42.0f, 34.0f, 25.0f, 0.7f, 0.7f, kInkDark, 0.48f);
        s.drawInkBezier(29.0f, 60.0f, 32.0f, 42.0f, 36.0f, 25.0f, 0.7f, 0.7f, kInkDark, 0.48f);
        for (float t = 0.15f; t < 0.95f; t += 0.12f) {
            float it = 1.0f - t;
            float px = it * it * 28.0f + 2.0f * it * t * 31.0f + t * t * 35.0f;
            float py = it * it * 60.0f + 2.0f * it * t * 42.0f + t * t * 25.0f;
            s.drawInkLine(px - 1.8f, py - 0.5f, px + 1.8f, py + 0.5f, 0.55f, kInkDark, 0.50f);
        }

        auto drawPalmFrond = [&](float x0, float y0, float cx, float cy, float x1, float y1) {
            s.drawInkBezier(x0, y0, cx, cy, x1, y1, 0.9f, 0.5f, kInkDark, 0.70f);
            for (int i = 1; i <= 8; ++i) {
                float t = static_cast<float>(i) / 9.0f;
                float it = 1.0f - t;
                float px = it * it * x0 + 2.0f * it * t * cx + t * t * x1;
                float py = it * it * y0 + 2.0f * it * t * cy + t * t * y1;
                float dx = 2.0f * it * (cx - x0) + 2.0f * t * (x1 - cx);
                float dy = 2.0f * it * (cy - y0) + 2.0f * t * (y1 - cy);
                float len = std::hypot(dx, dy);
                if (len > 0.1f) {
                    float nx = -dy / len * (4.5f * (1.0f - t * 0.4f));
                    float ny =  dx / len * (4.5f * (1.0f - t * 0.4f));
                    RGBA8 c = (i % 2 == 0) ? kPalmFrondLit : kPalmFrondMid;
                    s.drawInkLine(px, py, px + nx, py + ny, 0.6f, c, 0.68f);
                    s.drawInkLine(px, py, px - nx, py - ny, 0.6f, kPalmFrondDeep, 0.66f);
                }
            }
        };

        drawPalmFrond(35.0f, 25.0f, 20.0f, 15.0f,  8.0f, 28.0f);
        drawPalmFrond(35.0f, 25.0f, 24.0f, 10.0f, 16.0f, 18.0f);
        drawPalmFrond(35.0f, 25.0f, 34.0f,  6.0f, 32.0f, 10.0f);
        drawPalmFrond(35.0f, 25.0f, 46.0f, 10.0f, 52.0f, 18.0f);
        drawPalmFrond(35.0f, 25.0f, 52.0f, 16.0f, 58.0f, 28.0f);

    } else {
        // Style 3: Sorcery! / Watabou Minimalist Hand-Drawn Grass Tuft & Wildflowers
        auto drawCartoGrassTuft = [&](float bx, float by, float scale, uint32_t tuftSeed) {
            // 1. Soft watercolor sage pool at base
            for (int y = static_cast<int>(by - 4.0f); y <= static_cast<int>(by + 2.0f); ++y) {
                for (int x = static_cast<int>(bx - 7.0f * scale); x <= static_cast<int>(bx + 7.0f * scale); ++x) {
                    float dx = (static_cast<float>(x) - bx) / (7.0f * scale);
                    float dy = (static_cast<float>(y) - by) / 3.0f;
                    float d = dx * dx + dy * dy;
                    if (d <= 1.0f) {
                        float a = (1.0f - d) * 115.0f;
                        s.blendPixel(x, y, RGBA8{142, 196, 110, static_cast<uint8_t>(a)}, {0.0f, 0.3f, 0.95f}, 0.38f);
                    }
                }
            }

            // 2. Slender hand-inked blade curves (fountain pen strokes)
            constexpr RGBA8 kBladeInk{36, 52, 28, 255};
            s.drawInkBezier(bx, by, bx - 1.5f * scale, by - 10.0f * scale, bx - 2.5f * scale, by - 18.0f * scale,
                            1.2f, 0.5f, kBladeInk, 0.55f);
            s.drawInkBezier(bx, by, bx - 5.0f * scale, by - 8.0f * scale, bx - 9.0f * scale, by - 14.0f * scale,
                            1.1f, 0.45f, kBladeInk, 0.52f);
            s.drawInkBezier(bx, by, bx - 7.0f * scale, by - 4.0f * scale, bx - 11.5f * scale, by - 7.0f * scale,
                            1.0f, 0.4f, kBladeInk, 0.48f);
            s.drawInkBezier(bx, by, bx + 4.5f * scale, by - 7.5f * scale, bx + 8.5f * scale, by - 15.0f * scale,
                            1.1f, 0.45f, kBladeInk, 0.52f);
            s.drawInkBezier(bx, by, bx + 6.5f * scale, by - 4.0f * scale, bx + 11.0f * scale, by - 6.5f * scale,
                            1.0f, 0.4f, kBladeInk, 0.48f);

            // 3. Stipple ink dots around root
            s.drawInkDot(bx - 3.5f * scale, by + 0.5f, 0.55f, kInkDark, 0.36f);
            s.drawInkDot(bx + 4.0f * scale, by + 0.8f, 0.55f, kInkDark, 0.36f);
            s.drawInkDot(bx + 1.2f * scale, by + 1.2f, 0.55f, kInkDark, 0.36f);

            // 4. Charming hand-inked wildflower dots
            if ((tuftSeed & 1u) == 0u) {
                float fx = bx + 8.5f * scale;
                float fy = by - 15.0f * scale;
                s.drawInkDot(fx, fy, 1.4f, RGBA8{250, 218, 64, 255}, 0.85f);
                s.drawInkDot(fx, fy, 0.6f, kInkDark, 0.88f);
            } else {
                float fx = bx - 2.5f * scale;
                float fy = by - 18.0f * scale;
                s.drawInkDot(fx, fy, 1.4f, RGBA8{96, 160, 240, 255}, 0.85f);
                s.drawInkDot(fx, fy, 0.6f, kInkDark, 0.88f);
            }
        };

        drawCartoGrassTuft(25.0f, 57.0f, 1.15f, seed + 1u);
        drawCartoGrassTuft(43.0f, 58.0f, 0.90f, seed + 2u);
    }

    if (style != 0) {
        s.applyInkOutlineAndPaperCutout();
    }
}

// Synthesizes 2.5D Watabou-Style Tabletop Settlement Icons
// Crisp hipped/conical roofs with ink ridges, stone-hatched curtain walls, and amber-lit windows.
void synthSettlementStandee(LayerSlice& s, int kind) {
    s.clear();
    constexpr RGBA8 kWallLit    {244, 242, 238, 255};
    constexpr RGBA8 kWallMid    {216, 212, 204, 255};
    constexpr RGBA8 kWallShade  {176, 172, 164, 255};
    constexpr RGBA8 kRoofRedLit {218,  84,  56, 255};
    constexpr RGBA8 kRoofRedShd {162,  52,  34, 255};
    constexpr RGBA8 kRoofSlateLit{ 94, 118, 146, 255};
    constexpr RGBA8 kRoofSlateShd{ 62,  78, 102, 255};
    constexpr RGBA8 kWindowGold {255, 218,  84, 255};

    auto drawWatabouBuilding = [&](float x0, float x1, float roofTopY, float eaveY, float baseY,
                                   RGBA8 roofLit, RGBA8 roofShd, bool halfTimbered) {
        float midX = (x0 + x1) * 0.5f;
        int ix0 = static_cast<int>(x0), ix1 = static_cast<int>(x1);
        int ieave = static_cast<int>(eaveY), ibase = static_cast<int>(baseY);

        // 1. Facade Wall with stone masonry or plaster wash
        for (int y = ieave; y <= ibase; ++y) {
            for (int x = ix0; x <= ix1; ++x) {
                float dx = (static_cast<float>(x) - midX) / ((x1 - x0) * 0.5f);
                RGBA8 c = (dx < -0.1f) ? kWallLit : (dx > 0.2f ? kWallShade : kWallMid);
                s.blendPixel(x, y, c, {dx * 0.6f, 0.0f, 0.8f}, 0.62f);
            }
        }
        s.drawInkLine(x0, eaveY, x0, baseY, 0.8f, kInkDark, 0.65f);
        s.drawInkLine(x1, eaveY, x1, baseY, 0.8f, kInkDark, 0.65f);
        s.drawInkLine(x0, baseY, x1, baseY, 0.8f, kInkDark, 0.65f);

        if (halfTimbered) {
            float midBeamY = (eaveY + baseY) * 0.5f;
            s.drawInkLine(x0, midBeamY, x1, midBeamY, 0.8f, RGBA8{92, 64, 42, 255}, 0.68f);
            s.drawInkLine(x0 + 2.0f, eaveY, midX, midBeamY, 0.65f, RGBA8{92, 64, 42, 255}, 0.68f);
            s.drawInkLine(x1 - 2.0f, eaveY, midX, midBeamY, 0.65f, RGBA8{92, 64, 42, 255}, 0.68f);
        } else {
            for (float y = eaveY + 4.0f; y < baseY - 2.0f; y += 4.0f) {
                s.drawInkLine(x0 + 2.0f, y, x1 - 2.0f, y, 0.45f, kInkMid, 0.64f);
            }
        }

        // 2. Crisp Hipped/Pitched Roof
        int iroofTop = static_cast<int>(roofTopY);
        for (int y = iroofTop; y <= ieave; ++y) {
            float t = static_cast<float>(y - iroofTop) / std::max(1.0f, eaveY - roofTopY);
            float curHw = t * ((x1 - x0) * 0.5f + 2.5f);
            for (float x = midX - curHw; x <= midX + curHw; x += 0.7f) {
                float dx = (x - midX) / std::max(1.0f, curHw);
                RGBA8 c = (dx < 0.0f) ? roofLit : roofShd;
                s.blendPixel(static_cast<int>(x), y, c, {dx * 0.7f, 0.3f, 0.7f}, 0.78f);
            }
        }
        s.drawInkLine(midX, roofTopY, x0 - 2.0f, eaveY, 0.85f, kInkDark, 0.82f);
        s.drawInkLine(midX, roofTopY, x1 + 2.0f, eaveY, 0.85f, kInkDark, 0.82f);
        s.drawInkLine(x0 - 2.5f, eaveY, x1 + 2.5f, eaveY, 0.85f, kInkDark, 0.82f);
        s.drawInkLine(midX, roofTopY, midX, eaveY, 0.65f, kInkDark, 0.82f);

        // 3. Cozy Glowing Arched Window
        float winY = (eaveY + baseY) * 0.5f;
        s.fillCircle(midX, winY, 2.5f, kWindowGold, 0.75f, 0.95f);
        s.drawInkLine(midX - 2.5f, winY, midX + 2.5f, winY, 0.5f, kInkDark, 0.80f);
        s.drawInkLine(midX, winY - 2.5f, midX, winY + 2.5f, 0.5f, kInkDark, 0.80f);
    };

    auto drawRoundTower = [&](float cx, float topY, float botY, float r, RGBA8 roofLit, RGBA8 roofShd) {
        int itop = static_cast<int>(topY), ibot = static_cast<int>(botY);
        for (int y = itop; y <= ibot; ++y) {
            for (float x = cx - r; x <= cx + r; x += 0.7f) {
                float dx = (x - cx) / r;
                float nz = std::sqrt(std::max(0.1f, 1.0f - dx * dx));
                RGBA8 c = (dx < -0.2f) ? kWallLit : (dx > 0.2f ? kWallShade : kWallMid);
                s.blendPixel(static_cast<int>(x), y, c, {dx * 0.8f, 0.0f, nz}, 0.68f);
            }
        }
        s.drawInkLine(cx - r, topY, cx - r, botY, 0.85f, kInkDark, 0.72f);
        s.drawInkLine(cx + r, topY, cx + r, botY, 0.85f, kInkDark, 0.72f);

        // Crenellations
        for (float x = cx - r; x < cx + r; x += 3.0f) {
            s.drawInkLine(x, topY - 2.5f, x + 1.5f, topY - 2.5f, 0.8f, kWallLit, 0.78f);
        }

        // Conical Roof Cap
        float capTopY = topY - r * 1.6f;
        int icapTop = static_cast<int>(capTopY);
        for (int y = icapTop; y <= itop; ++y) {
            float t = static_cast<float>(y - icapTop) / std::max(1.0f, topY - capTopY);
            float curR = t * (r + 1.5f);
            for (float x = cx - curR; x <= cx + curR; x += 0.7f) {
                float dx = (x - cx) / std::max(1.0f, curR);
                RGBA8 c = (dx < 0.0f) ? roofLit : roofShd;
                s.blendPixel(static_cast<int>(x), y, c, {dx * 0.7f, 0.3f, 0.7f}, 0.84f);
            }
        }
        s.drawInkLine(cx, capTopY, cx - r - 1.5f, topY, 0.85f, kInkDark, 0.86f);
        s.drawInkLine(cx, capTopY, cx + r + 1.5f, topY, 0.85f, kInkDark, 0.86f);
        s.drawInkLine(cx - r - 1.5f, topY, cx + r + 1.5f, topY, 0.85f, kInkDark, 0.86f);
    };

    if (kind == 0) {
        // Watabou Citadel Keep: Central high keep flanked by twin crenellated round towers
        drawRoundTower(16.0f, 26.0f, 60.0f, 6.5f, kRoofSlateLit, kRoofSlateShd);
        drawRoundTower(48.0f, 26.0f, 60.0f, 6.5f, kRoofSlateLit, kRoofSlateShd);
        drawWatabouBuilding(18.0f, 46.0f, 8.0f, 22.0f, 60.0f, kRoofRedLit, kRoofRedShd, false);
        // Central portcullis gate arch
        s.fillCircle(32.0f, 52.0f, 5.0f, kInkDark, 0.70f);
        s.drawInkLine(27.0f, 52.0f, 27.0f, 60.0f, 0.8f, kInkDark, 0.72f);
        s.drawInkLine(37.0f, 52.0f, 37.0f, 60.0f, 0.8f, kInkDark, 0.72f);
        // Keep peak heraldic pennant
        s.drawInkLine(32.0f, 2.0f, 32.0f, 8.0f, 0.9f, kInkDark, 0.90f);
        s.drawInkBezier(32.0f, 2.0f, 38.0f, 1.5f, 44.0f, 4.0f, 1.1f, 0.5f, kRoofRedLit, 0.92f);

    } else if (kind == 1) {
        // Hamlet Gabled Cottages (Watabou half-timbered village style)
        drawWatabouBuilding(8.0f, 32.0f, 22.0f, 35.0f, 60.0f, kRoofRedLit, kRoofRedShd, true);
        drawWatabouBuilding(28.0f, 54.0f, 16.0f, 30.0f, 60.0f, kRoofRedLit, kRoofRedShd, true);
        s.drawInkLine(14.0f, 20.0f, 14.0f, 26.0f, 1.4f, kWallShade, 0.82f);
        s.drawInkBezier(14.0f, 19.0f, 11.0f, 13.0f, 17.0f, 8.0f, 0.7f, 1.3f, RGBA8{210, 214, 220, 180}, 0.88f);

    } else if (kind == 2) {
        // Watabou Rural Timber Windmill
        drawRoundTower(32.0f, 22.0f, 60.0f, 10.0f, kRoofRedLit, kRoofRedShd);
        constexpr RGBA8 kSailWhite{246, 244, 238, 255};
        auto drawLatticeSail = [&](float x0, float y0, float x1, float y1) {
            s.drawInkLine(x0, y0, x1, y1, 1.1f, RGBA8{102, 70, 44, 255}, 0.92f);
            s.drawInkLine(x0, y0, x1, y1, 0.6f, kSailWhite, 0.94f);
            for (float t = 0.25f; t < 0.95f; t += 0.22f) {
                float px = x0 + (x1 - x0) * t;
                float py = y0 + (y1 - y0) * t;
                float dx = (y1 - y0) * 0.12f;
                float dy = -(x1 - x0) * 0.12f;
                s.drawInkLine(px - dx, py - dy, px + dx, py + dy, 0.5f, kSailWhite, 0.95f);
            }
        };
        drawLatticeSail(32.0f, 20.0f, 10.0f, 10.0f);
        drawLatticeSail(32.0f, 20.0f, 54.0f, 10.0f);
        drawLatticeSail(32.0f, 20.0f, 54.0f, 38.0f);
        drawLatticeSail(32.0f, 20.0f, 10.0f, 38.0f);
        s.fillCircle(32.0f, 20.0f, 2.5f, RGBA8{82, 54, 32, 255}, 0.98f);

    } else if (kind == 3) {
        // Coastal Beacon Lighthouse
        for (int y = 14; y <= 60; ++y) {
            float t = static_cast<float>(y - 14) / 46.0f;
            float hw = 4.5f + t * 6.5f;
            bool stripe = ((y / 8) % 2 == 0);
            RGBA8 c = stripe ? kWallLit : kRoofRedLit;
            for (float x = 32.0f - hw; x <= 32.0f + hw; x += 0.7f) {
                float dx = (x - 32.0f) / hw;
                s.blendPixel(static_cast<int>(x), y, c, {dx * 0.7f, 0.0f, 0.7f}, 0.75f);
            }
        }
        s.drawInkLine(27.5f, 14.0f, 21.0f, 60.0f, 0.85f, kInkDark, 0.78f);
        s.drawInkLine(36.5f, 14.0f, 43.0f, 60.0f, 0.85f, kInkDark, 0.78f);
        s.fillCircle(32.0f, 11.5f, 5.0f, kWindowGold, 0.95f, 1.0f);
        drawWatabouBuilding(25.0f, 39.0f, 4.0f, 8.5f, 11.0f, kRoofSlateLit, kRoofSlateShd, false);

    } else if (kind == 4) {
        // Alpine Sanctuary Monastery Spire
        drawWatabouBuilding(18.0f, 46.0f, 24.0f, 38.0f, 60.0f, kRoofSlateLit, kRoofSlateShd, false);
        drawWatabouBuilding(27.0f, 37.0f,  4.0f, 22.0f, 38.0f, kRoofSlateLit, kRoofSlateShd, false);
        s.fillCircle(32.0f, 30.0f, 3.5f, kWindowGold, 0.85f, 0.90f);

    } else {
        // Port Harbor Pier & Timber Warehouse
        drawWatabouBuilding(12.0f, 42.0f, 18.0f, 30.0f, 54.0f, kRoofRedLit, kRoofRedShd, true);
        s.drawInkLine(6.0f, 54.0f, 58.0f, 54.0f, 1.8f, RGBA8{118, 82, 54, 255}, 0.55f);
        for (float px = 10.0f; px <= 54.0f; px += 10.0f) {
            s.drawInkLine(px, 54.0f, px, 61.0f, 1.2f, RGBA8{78, 52, 32, 255}, 0.52f);
        }
    }

    s.drawInkLine(6.0f, 60.0f, 58.0f, 60.0f, 1.0f, kInkDark, 0.40f);
    s.applyInkOutlineAndPaperCutout();
}

// Synthesizes Hand-Drawn Sorcery! Tabletop Adventurer Pawn, Antique Caravel, Waypoint Flags & Dungeon Props
void synthCharacterOrPropStandee(LayerSlice& s, uint16_t layer) {
    s.clear();
    if (layer == InkLayer::StandeeHeroCat) {
        // Sorcery!-Style Tabletop Pewter Adventurer Miniature Pawn on Turned Wood/Bronze Counter Base
        constexpr RGBA8 kBaseRimLit {210, 162, 108, 255};
        constexpr RGBA8 kBaseWoodMid{148, 104,  64, 255};
        constexpr RGBA8 kBaseWoodDrk{ 86,  56,  32, 255};

        // 1. Turned wooden/bronze token counter base
        for (int y = 57; y <= 62; ++y) {
            for (int x = 16; x <= 48; ++x) {
                float dx = (static_cast<float>(x) - 32.0f) / 15.0f;
                float dy = (static_cast<float>(y) - 59.5f) / 2.5f;
                if (dx * dx + dy * dy <= 1.0f) {
                    s.blendPixel(x, y, RGBA8{20, 15, 10, 160}, {0.0f, 1.0f, 0.0f}, 0.20f);
                }
            }
        }
        for (int y = 52; y <= 60; ++y) {
            for (int x = 18; x <= 46; ++x) {
                float dx = (static_cast<float>(x) - 32.0f) / 13.5f;
                float dy = (static_cast<float>(y) - 56.0f) / 4.0f;
                float d2 = dx * dx + dy * dy;
                if (d2 <= 1.0f) {
                    float nz = std::sqrt(std::max(0.1f, 1.0f - d2));
                    float t = (dy * 0.6f + dx * 0.4f);
                    RGBA8 c = kBaseWoodMid;
                    if (t < -0.15f) c = kBaseRimLit;
                    else if (t > 0.15f) c = kBaseWoodDrk;
                    s.blendPixel(x, y, c, {dx * 0.7f, -dy * 0.7f, nz}, 0.42f + nz * 0.18f);
                }
            }
        }
        s.drawInkBezier(18.5f, 56.0f, 32.0f, 60.5f, 45.5f, 56.0f, 0.9f, 0.9f, kInkDark, 0.55f);
        s.drawInkBezier(18.5f, 56.0f, 32.0f, 51.5f, 45.5f, 56.0f, 0.7f, 0.7f, kInkDark, 0.55f);
        s.drawInkBezier(21.0f, 55.5f, 32.0f, 58.5f, 43.0f, 55.5f, 0.5f, 0.5f, kBaseRimLit, 0.58f);

        // 2. Pewter Adventurer Figurine (Silver/steel miniature with pen hatching)
        constexpr RGBA8 kPewterHi   {248, 250, 255, 255};
        constexpr RGBA8 kPewterLit  {216, 224, 234, 255};
        constexpr RGBA8 kPewterMid  {164, 176, 192, 255};
        constexpr RGBA8 kPewterShade{108, 120, 136, 255};
        constexpr RGBA8 kCloakBlue  { 56,  82, 128, 255};
        constexpr RGBA8 kCloakShade { 32,  48,  82, 255};

        // Boots & Greaves
        s.drawInkBezier(28.0f, 44.0f, 27.5f, 50.0f, 26.5f, 55.5f, 2.0f, 2.4f, kPewterMid, 0.65f);
        s.drawInkBezier(35.0f, 44.0f, 35.5f, 50.0f, 36.5f, 55.5f, 2.0f, 2.4f, kPewterMid, 0.65f);
        s.drawInkLine(24.5f, 55.5f, 28.5f, 55.5f, 0.8f, kInkDark, 0.68f);
        s.drawInkLine(34.5f, 55.5f, 38.5f, 55.5f, 0.8f, kInkDark, 0.68f);

        // Flowing Traveler Cloak
        for (int y = 20; y <= 48; ++y) {
            float t = static_cast<float>(y - 20) / 28.0f;
            float leftW = 5.0f + t * 9.5f;
            float rightW = 4.0f + t * 4.0f;
            for (float x = 32.0f - leftW; x <= 32.0f + rightW; x += 0.7f) {
                float dx = (x - 32.0f) / leftW;
                RGBA8 c = (dx < -0.2f) ? kCloakBlue : kCloakShade;
                s.blendPixel(static_cast<int>(x), y, c, {-0.3f, 0.2f, 0.9f}, 0.55f);
            }
        }
        s.drawInkBezier(29.0f, 20.0f, 24.0f, 34.0f, 18.0f, 48.0f, 0.8f, 1.1f, kInkDark, 0.58f);
        s.drawInkBezier(31.0f, 22.0f, 27.0f, 36.0f, 23.0f, 48.0f, 0.6f, 0.8f, kInkDark, 0.58f);
        s.drawInkBezier(18.0f, 48.0f, 26.0f, 49.5f, 34.0f, 47.0f, 0.7f, 0.7f, kInkDark, 0.58f);

        // Armored Torso / Mail Hauberk
        for (int y = 24; y <= 40; ++y) {
            int hw = (y < 32) ? 6 : 5;
            for (int x = 32 - hw; x <= 32 + hw; ++x) {
                float dx = static_cast<float>(x - 32) / static_cast<float>(hw);
                RGBA8 c = (dx < -0.2f) ? kPewterLit : (dx > 0.2f ? kPewterShade : kPewterMid);
                s.blendPixel(x, y, c, {dx * 0.7f, 0.0f, 0.7f}, 0.72f);
            }
        }
        s.drawInkLine(27.0f, 37.0f, 37.0f, 37.0f, 1.0f, RGBA8{78, 52, 34, 255}, 0.74f);
        s.drawInkDot(32.0f, 37.0f, 1.2f, RGBA8{236, 186, 68, 255}, 0.78f);

        // Pewter Pauldrons
        s.fillCircle(25.5f, 24.0f, 4.0f, kPewterLit, 0.82f);
        s.fillCircle(38.5f, 24.0f, 4.0f, kPewterMid, 0.80f);
        s.drawInkDot(24.5f, 22.5f, 1.2f, kPewterHi, 0.88f);
        s.drawInkDot(37.5f, 22.5f, 1.0f, kPewterHi, 0.86f);

        // Bascinet Helm / Hood
        s.fillCircle(32.0f, 16.0f, 5.0f, kPewterMid, 0.84f);
        s.drawInkDot(30.5f, 14.5f, 1.4f, kPewterHi, 0.90f);
        s.drawInkLine(30.0f, 16.5f, 34.0f, 16.5f, 0.65f, kInkDark, 0.86f);

        // Hero's Drawn Broadsword
        s.drawInkDot(39.0f, 32.0f, 1.5f, kPewterShade, 0.78f);
        s.drawInkLine(36.5f, 30.5f, 42.0f, 33.5f, 1.0f, RGBA8{214, 172, 78, 255}, 0.80f);
        s.drawInkDot(36.0f, 33.0f, 1.0f, RGBA8{214, 172, 78, 255}, 0.80f);
        s.drawInkLine(40.0f, 31.0f, 48.0f, 11.0f, 1.3f, kPewterLit, 0.88f);
        s.drawInkLine(39.5f, 31.0f, 47.5f, 11.0f, 0.6f, kPewterHi, 0.92f);
        s.drawInkLine(40.5f, 31.0f, 48.5f, 11.0f, 0.5f, kPewterShade, 0.88f);
        s.drawInkDot(48.0f, 11.0f, 0.8f, kPewterHi, 0.98f);

    } else if (layer == InkLayer::StandeeHeroCompanion) {
        // Floating Arcane Spirit Familiar
        s.fillCircle(32.0f, 30.0f, 9.0f, RGBA8{110, 205, 255, 255}, 0.9f, 0.85f);
        s.fillCircle(32.0f, 30.0f, 5.0f, RGBA8{245, 250, 255, 255}, 0.95f, 1.0f);
        s.drawInkBezier(24.0f, 38.0f, 32.0f, 44.0f, 40.0f, 38.0f, 0.8f, 0.8f, RGBA8{140, 225, 255, 255}, 0.90f);

    } else if (layer == InkLayer::StandeeShipCaravel) {
        // Hand-Inked Antique Caravel Flagship (Woodcut style with billowing canvas sails & rigging)
        constexpr RGBA8 kHullMid     {128,  84,  52, 255};
        constexpr RGBA8 kHullShade   { 84,  52,  30, 255};
        constexpr RGBA8 kSailCream   {244, 238, 222, 255};
        constexpr RGBA8 kSailShadow  {204, 194, 172, 255};
        constexpr RGBA8 kLanternGold {255, 218,  78, 255};

        // 1. Waterline keel & hull
        for (int y = 43; y <= 57; ++y) {
            float t = static_cast<float>(y - 43) / 14.0f;
            float bowX = 52.0f - t * 4.0f;
            float sternX = 14.0f + t * 5.0f;
            for (float x = sternX; x <= bowX; x += 0.7f) {
                RGBA8 c = (y > 52) ? kHullShade : kHullMid;
                s.blendPixel(static_cast<int>(x), y, c, {0.0f, -0.2f, 0.9f}, 0.60f);
            }
        }
        s.drawInkBezier(14.0f, 43.0f, 32.0f, 46.0f, 52.0f, 43.0f, 0.9f, 0.9f, kInkDark, 0.65f);
        s.drawInkBezier(16.0f, 48.0f, 33.0f, 51.0f, 50.0f, 47.0f, 0.8f, 0.8f, kInkDark, 0.65f);
        s.drawInkBezier(19.0f, 54.0f, 34.0f, 56.5f, 48.0f, 53.0f, 0.9f, 0.9f, kInkDark, 0.65f);
        s.drawInkLine(18.0f, 57.0f, 26.0f, 57.0f, 0.7f, RGBA8{248, 252, 255, 255}, 0.50f);
        s.drawInkLine(32.0f, 57.5f, 42.0f, 57.5f, 0.7f, RGBA8{248, 252, 255, 255}, 0.50f);

        // 2. Masts & Rigging
        s.drawInkLine(32.0f, 10.0f, 32.0f, 46.0f, 1.2f, kHullShade, 0.75f);
        s.drawInkLine(45.0f, 18.0f, 45.0f, 46.0f, 0.9f, kHullShade, 0.72f);
        s.drawInkLine(52.0f, 43.0f, 60.0f, 35.0f, 0.9f, kHullShade, 0.70f);
        s.drawInkLine(32.0f, 14.0f, 20.0f, 44.0f, 0.5f, kInkDark, 0.70f);
        s.drawInkLine(32.0f, 14.0f, 42.0f, 44.0f, 0.5f, kInkDark, 0.70f);

        // 3. Billowing Canvas Mainsail
        for (int y = 16; y <= 38; ++y) {
            float t = static_cast<float>(y - 16) / 22.0f;
            float hw = 10.0f + std::sin(t * 3.14159f) * 3.5f;
            for (float x = 32.0f - hw; x <= 32.0f + hw; x += 0.7f) {
                float dx = (x - 32.0f) / hw;
                float dome = 1.0f - dx * dx;
                RGBA8 c = (dx < -0.15f) ? kSailCream : kSailShadow;
                s.blendPixel(static_cast<int>(x), y, c, {dx * 0.7f, -0.2f, dome}, 0.70f + dome * 0.15f);
            }
        }
        s.drawInkLine(22.0f, 16.0f, 42.0f, 16.0f, 1.0f, kHullShade, 0.78f);
        s.drawInkLine(20.0f, 38.0f, 44.0f, 38.0f, 0.9f, kHullShade, 0.78f);
        s.drawInkBezier(22.0f, 16.0f, 19.0f, 27.0f, 20.0f, 38.0f, 0.8f, 0.8f, kInkDark, 0.82f);
        s.drawInkBezier(42.0f, 16.0f, 45.0f, 27.0f, 44.0f, 38.0f, 0.8f, 0.8f, kInkDark, 0.82f);
        // Heraldic red cross
        s.drawInkLine(32.0f, 21.0f, 32.0f, 33.0f, 1.4f, RGBA8{204, 52, 42, 255}, 0.85f);
        s.drawInkLine(26.0f, 26.0f, 38.0f, 26.0f, 1.4f, RGBA8{204, 52, 42, 255}, 0.85f);

        // 4. Glowing Aft Beacon Lantern
        s.drawInkLine(14.0f, 40.0f, 10.0f, 41.0f, 0.8f, kInkDark, 0.85f);
        s.fillCircle(9.5f, 41.5f, 2.8f, kLanternGold, 0.95f, 1.0f);
        s.drawInkDot(9.5f, 41.5f, 0.8f, RGBA8{255, 252, 220, 255}, 1.0f);

        // 5. Pennant
        s.drawInkBezier(32.0f, 10.0f, 39.0f, 8.5f, 46.0f, 11.5f, 1.3f, 0.5f, RGBA8{214, 58, 48, 255}, 0.85f);

    } else if (layer == InkLayer::StandeeSignpost) {
        // Waypoint Pennant Flag on Wooden Stake (Sorcery! style travel marker)
        s.drawInkBezier(32.0f, 20.0f, 32.0f, 40.0f, 32.0f, 60.0f, 1.4f, 1.8f, RGBA8{128, 86, 56, 255}, 0.55f);
        s.drawInkDot(32.0f, 19.0f, 2.0f, RGBA8{214, 182, 88, 255}, 0.80f);
        s.fillCircle(32.0f, 58.0f, 5.0f, RGBA8{168, 172, 178, 255}, 0.50f);
        s.drawInkBezier(27.0f, 58.0f, 32.0f, 61.0f, 37.0f, 58.0f, 0.8f, 0.8f, kInkDark, 0.52f);

        constexpr RGBA8 kFlagBlueLit  { 58, 122, 224, 255};
        constexpr RGBA8 kFlagBlueMid  { 38,  88, 186, 255};
        constexpr RGBA8 kFlagBlueShade{ 22,  56, 136, 255};

        for (int y = 20; y <= 35; ++y) {
            float t = static_cast<float>(y - 20) / 15.0f;
            float flagLen = (1.0f - t * 0.45f) * 24.0f;
            for (float x = 32.0f; x <= 32.0f + flagLen; x += 0.7f) {
                float fx = (x - 32.0f) / flagLen;
                float wave = std::sin(fx * 3.14159f * 1.5f);
                RGBA8 c = (wave > 0.1f) ? kFlagBlueLit : ((wave < -0.1f) ? kFlagBlueShade : kFlagBlueMid);
                s.blendPixel(static_cast<int>(x), y, c, {0.0f, 0.2f, 0.9f}, 0.72f);
            }
        }
        s.drawInkBezier(32.0f, 20.0f, 44.0f, 19.0f, 56.0f, 21.0f, 0.8f, 0.8f, kInkDark, 0.76f);
        s.drawInkBezier(32.0f, 35.0f, 40.0f, 33.5f, 48.0f, 35.0f, 0.8f, 0.8f, kInkDark, 0.76f);
        s.drawInkLine(56.0f, 21.0f, 48.0f, 35.0f, 0.8f, kInkDark, 0.76f);

    } else if (layer == InkLayer::StandeeDungeonArch) {
        // Skull Dungeon Fort Portal (Sorcery! style)
        s.fillCircle(32.0f, 38.0f, 19.0f, RGBA8{228, 232, 238, 255}, 0.7f);
        s.fillRect(14, 38, 50, 60, RGBA8{205, 214, 224, 255}, 0.7f);
        s.fillCircle(32.0f, 32.0f, 10.5f, RGBA8{252, 252, 255, 255}, 0.85f);
        s.fillRect(26, 38, 38, 48, RGBA8{252, 252, 255, 255}, 0.85f);
        s.fillCircle(28.5f, 33.0f, 2.6f, kInkDark, 0.95f);
        s.fillCircle(35.5f, 33.0f, 2.6f, kInkDark, 0.95f);
        s.fillRect(31, 37, 33, 40, kInkDark, 0.95f);
        s.drawLine(27.0f, 44.0f, 37.0f, 44.0f, 1.2f, kInkDark, 0.95f);
        for (int tx = 28; tx <= 36; tx += 2) {
            s.drawLine(static_cast<float>(tx), 42.0f, static_cast<float>(tx), 46.0f, 0.9f, kInkDark, 0.9f);
        }
        s.drawLine(18.0f, 55.0f, 46.0f, 55.0f, 2.4f, RGBA8{248, 248, 252, 255}, 0.85f);

    } else if (layer == InkLayer::StandeeMonsterSlime) {
        // Hand-Inked Arcane Slime
        s.fillCircle(32.0f, 45.0f, 13.0f, RGBA8{82, 188, 142, 255}, 0.8f, 0.2f);
        s.drawInkBezier(19.0f, 45.0f, 32.0f, 32.0f, 45.0f, 45.0f, 0.9f, 0.9f, kInkDark, 0.85f);
        s.drawInkBezier(19.0f, 45.0f, 32.0f, 58.0f, 45.0f, 45.0f, 0.9f, 0.9f, kInkDark, 0.85f);
        s.fillCircle(28.0f, 43.0f, 2.5f, kInkDark, 0.95f);
        s.fillCircle(36.0f, 43.0f, 2.5f, kInkDark, 0.95f);
        s.drawInkDot(27.5f, 42.5f, 0.8f, RGBA8{255, 255, 255, 255}, 1.0f);
        s.drawInkDot(35.5f, 42.5f, 0.8f, RGBA8{255, 255, 255, 255}, 1.0f);

    } else if (layer == InkLayer::StandeeMonsterBandit) {
        // Hand-Inked Highwayman
        s.fillCircle(32.0f, 22.0f, 8.5f, RGBA8{198, 152, 118, 255}, 0.85f);
        s.drawInkBezier(22.0f, 18.0f, 32.0f, 13.0f, 42.0f, 18.0f, 1.2f, 1.2f, kInkDark, 0.90f);
        s.drawInkLine(22.0f, 25.0f, 42.0f, 25.0f, 1.6f, RGBA8{48, 42, 38, 255}, 0.88f); // Mask
        for (int y = 30; y <= 58; ++y) {
            for (int x = 22; x <= 42; ++x) {
                s.blendPixel(x, y, RGBA8{142, 64, 54, 255}, {0.0f, 0.0f, 0.8f}, 0.75f);
            }
        }
        s.drawInkLine(42.0f, 40.0f, 56.0f, 20.0f, 1.5f, RGBA8{214, 222, 232, 255}, 0.88f); // Cutlass

    } else if (layer == InkLayer::StandeeMonsterWyvern) {
        // Hand-Inked Dragon / Sea Wyvern Boss
        s.drawInkBezier(10.0f, 16.0f, 24.0f, 26.0f, 32.0f, 34.0f, 2.5f, 3.5f, RGBA8{176, 56, 48, 255}, 0.8f);
        s.drawInkBezier(54.0f, 16.0f, 40.0f, 26.0f, 32.0f, 34.0f, 2.5f, 3.5f, RGBA8{176, 56, 48, 255}, 0.8f);
        s.fillCircle(32.0f, 38.0f, 13.0f, RGBA8{148, 42, 38, 255}, 0.85f);
        s.fillCircle(32.0f, 18.0f,  9.0f, RGBA8{196, 68, 52, 255}, 0.9f);
        s.fillCircle(29.0f, 16.0f,  2.2f, RGBA8{255, 220, 80, 255}, 0.95f, 1.0f);
        s.fillCircle(35.0f, 16.0f,  2.2f, RGBA8{255, 220, 80, 255}, 0.95f, 1.0f);
        s.drawInkDot(29.0f, 16.0f, 0.8f, kInkDark, 1.0f);
        s.drawInkDot(35.0f, 16.0f, 0.8f, kInkDark, 1.0f);

    } else if (layer == InkLayer::StandeeSlashArc) {
        // Glowing crescent sword slash arc
        for (int y = 8; y < 56; ++y) {
            for (int x = 8; x < 56; ++x) {
                float dx = static_cast<float>(x - 32);
                float dy = static_cast<float>(y - 32);
                float r = std::sqrt(dx * dx + dy * dy);
                if (r > 15.0f && r < 24.0f && dx > -6.0f) {
                    s.setPixel(x, y, RGBA8{255, 245, 200, 255}, 0.9f, 1.0f);
                }
            }
        }

    } else if (layer == InkLayer::StandeeStandingStone) {
        // Hand-Carved Monolith with Glowing Blue Runes
        for (int y = 14; y <= 58; ++y) {
            for (int x = 24; x <= 40; ++x) {
                float dx = (static_cast<float>(x) - 32.0f) / 8.0f;
                RGBA8 c = (dx < -0.1f) ? RGBA8{172, 178, 186, 255} : RGBA8{124, 128, 136, 255};
                s.blendPixel(x, y, c, {dx * 0.7f, 0.0f, 0.7f}, 0.80f);
            }
        }
        s.drawInkBezier(24.0f, 14.0f, 32.0f, 12.0f, 40.0f, 14.0f, 0.9f, 0.9f, kInkDark, 0.85f);
        s.drawInkLine(24.0f, 14.0f, 24.0f, 58.0f, 0.9f, kInkDark, 0.85f);
        s.drawInkLine(40.0f, 14.0f, 40.0f, 58.0f, 0.9f, kInkDark, 0.85f);
        // Glowing carved arcane rune
        s.drawInkLine(32.0f, 22.0f, 32.0f, 42.0f, 1.2f, RGBA8{110, 220, 255, 255}, 0.95f);
        s.drawInkLine(28.0f, 28.0f, 36.0f, 28.0f, 1.2f, RGBA8{110, 220, 255, 255}, 0.95f);
        s.drawInkLine(29.0f, 36.0f, 35.0f, 36.0f, 1.2f, RGBA8{110, 220, 255, 255}, 0.95f);
        s.fillCircle(32.0f, 30.0f, 2.5f, RGBA8{230, 250, 255, 255}, 1.0f, 1.0f);

    } else if (layer == InkLayer::StandeeTreasureChest) {
        // Gilded Oak Chest with Iron Bands
        for (int y = 34; y <= 58; ++y) {
            for (int x = 16; x <= 48; ++x) {
                s.blendPixel(x, y, RGBA8{142, 94, 52, 255}, {0.0f, 0.0f, 0.8f}, 0.75f);
            }
        }
        s.drawInkLine(16.0f, 34.0f, 48.0f, 34.0f, 1.2f, kInkDark, 0.82f);
        s.drawInkLine(16.0f, 43.0f, 48.0f, 43.0f, 1.4f, RGBA8{62, 62, 68, 255}, 0.85f); // Iron band
        s.drawInkLine(24.0f, 34.0f, 24.0f, 58.0f, 1.2f, RGBA8{62, 62, 68, 255}, 0.85f);
        s.drawInkLine(40.0f, 34.0f, 40.0f, 58.0f, 1.2f, RGBA8{62, 62, 68, 255}, 0.85f);
        s.fillCircle(32.0f, 44.0f, 3.0f, RGBA8{242, 198, 72, 255}, 0.95f, 0.7f); // Gold keyhole lock

    } else if (layer == InkLayer::StandeeCampfire) {
        // Campfire with Crossed Inked Logs and Flickering Hearth Flames
        s.drawInkLine(18.0f, 56.0f, 46.0f, 48.0f, 2.5f, RGBA8{112, 72, 42, 255}, 0.5f);
        s.drawInkLine(18.0f, 48.0f, 46.0f, 56.0f, 2.5f, RGBA8{112, 72, 42, 255}, 0.5f);
        s.fillCircle(32.0f, 40.0f, 9.0f, RGBA8{255, 165, 48, 255}, 0.9f, 1.0f);
        s.fillCircle(32.0f, 42.0f, 5.0f, RGBA8{255, 238, 128, 255}, 0.95f, 1.0f);
        s.drawInkDot(30.0f, 32.0f, 1.0f, RGBA8{255, 200, 60, 255}, 0.95f); // Ember
        s.drawInkDot(35.0f, 28.0f, 0.8f, RGBA8{255, 160, 40, 255}, 0.95f);

    } else {
        // Fallback / Ancient Monument
        s.fillRect(22, 18, 42, 60, kPaperShadow, 0.7f);
    }
    s.applyInkOutlineAndPaperCutout();
}

} // namespace

InkAtlasData InkCanvas::generateFullAtlas(uint32_t seed) {
    InkAtlasData atlas{};
    const size_t totalBytes = static_cast<size_t>(kS) * kS * InkLayer::kCount * 4;
    atlas.albedoBytes.resize(totalBytes, 0);
    atlas.normalBytes.resize(totalBytes, 0);

    auto slicePtr = std::make_unique<LayerSlice>();
    LayerSlice& slice = *slicePtr;

    for (uint32_t layer = 0; layer < InkLayer::kCount; ++layer) {
        slice.clear();
        switch (layer) {
            case InkLayer::ParchmentGrain:
            case InkLayer::CobblePlaza:
            case InkLayer::TimberBridge:
            case InkLayer::DungeonFloorTile:
            case InkLayer::DungeonWallHatch:
                slice.clear(kPaperCream, 0.5f);
                for (int y = 0; y < kS; ++y) {
                    for (int x = 0; x < kS; ++x) {
                        float n = rand01(x, y, seed + layer * 37u);
                        uint8_t d = static_cast<uint8_t>(n * 14.0f);
                        RGBA8 c{
                            static_cast<uint8_t>(kPaperCream.r - d),
                            static_cast<uint8_t>(kPaperCream.g - d),
                            static_cast<uint8_t>(kPaperCream.b - d),
                            255
                        };
                        if (layer == InkLayer::DungeonWallHatch && ((x + y) % 5 == 0 || (x - y + 64) % 7 == 0)) {
                            c = kInkMid;
                        }
                        slice.setPixel(x, y, c, 0.4f + n * 0.2f);
                    }
                }
                break;

            case InkLayer::InkCalligraphyInk:
                // Deep embossed India-ink for 3D map-surface calligraphic banners
                slice.clear(kInkDark, 0.85f);
                break;

            case InkLayer::StandeeRockPillar:
                synthRockFormationStandee(slice, seed + 1u, 0);
                break;
            case InkLayer::StandeeRockOutcrop:
                synthRockFormationStandee(slice, seed + 2u, 1);
                break;
            case InkLayer::StandeeSeaStack:
                synthRockFormationStandee(slice, seed + 3u, 2);
                break;

            case InkLayer::StandeeForestOak:
                synthForestCanopy(slice, seed + 10u, 0);
                break;
            case InkLayer::StandeeForestPine:
                synthForestCanopy(slice, seed + 11u, 1);
                break;
            case InkLayer::StandeeForestPalm:
                synthForestCanopy(slice, seed + 12u, 2);
                break;
            case InkLayer::StandeeGrassPatch:
                synthForestCanopy(slice, seed + 13u, 3);
                break;

            case InkLayer::StandeeTownKeep:
                synthSettlementStandee(slice, 0);
                break;
            case InkLayer::StandeeHamletCottage:
                synthSettlementStandee(slice, 1);
                break;
            case InkLayer::StandeeWindmill:
                synthSettlementStandee(slice, 2);
                break;
            case InkLayer::StandeeLighthouse:
                synthSettlementStandee(slice, 3);
                break;
            case InkLayer::StandeeMonastery:
                synthSettlementStandee(slice, 4);
                break;
            case InkLayer::StandeePortHarbor:
                synthSettlementStandee(slice, 5);
                break;

            default:
                synthCharacterOrPropStandee(slice, static_cast<uint16_t>(layer));
                break;
        }
        slice.bakeNormalsToAtlas(layer, atlas);
    }

    return atlas;
}

} // namespace eatscharted
