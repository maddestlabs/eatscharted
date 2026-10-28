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

// Realms-style 2.5D Volumetric Foliage & Grass Patch Synthesizer
// Generates rich leaf clusters with 3D spherical/tiered normals, self-shadowed interior AO,
// and natural organic silhouettes (no flat black cartoon outlines).
void synthForestCanopy(LayerSlice& s, uint32_t seed, int style) {
    s.clear();
    // style 0: Realms 2.5D Multi-Tiered Deciduous Oak Tree
    // style 1: Realms 2.5D Layered Boreal Pine Tree
    // style 2: Coastal Palm & Tropical Shrub Cluster
    // style 3: Realms 2.5D Lush Grass Blades & Wildflower Patch

    auto paintLeafCluster = [&](float cx, float cy, float rx, float ry,
                                RGBA8 colLit, RGBA8 colMid, RGBA8 colDeep,
                                float zBias, uint32_t clusterSeed) {
        int x0 = std::max(1, static_cast<int>(cx - rx - 3.0f));
        int x1 = std::min(kS - 2, static_cast<int>(cx + rx + 3.0f));
        int y0 = std::max(1, static_cast<int>(cy - ry - 3.0f));
        int y1 = std::min(kS - 2, static_cast<int>(cy + ry + 3.0f));

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                float dx = (static_cast<float>(x) + 0.5f - cx) / rx;
                float dy = (static_cast<float>(y) + 0.5f - cy) / ry;
                float angle = std::atan2(dy, dx);
                // Organic leaf-clump scalloped edge modulation
                float leafJag = 0.14f * std::sin(angle * 7.0f + static_cast<float>(clusterSeed & 15u)) +
                                0.09f * std::cos(angle * 13.0f + static_cast<float>(clusterSeed >> 4u));
                float d2 = dx * dx + dy * dy;
                float maxR = 1.0f + leafJag;
                if (d2 <= maxR * maxR) {
                    float rNorm = std::sqrt(d2) / maxR;
                    float nz = std::sqrt(std::max(0.08f, 1.0f - rNorm * rNorm));
                    // 3D normal + individual leaf flutter perturbation
                    float microX = (rand01(x, y, clusterSeed + 1u) - 0.5f) * 0.35f;
                    float microY = (rand01(x, y, clusterSeed + 2u) - 0.5f) * 0.35f;
                    Vec3f norm{dx * 0.78f + microX, -dy * 0.78f + microY, nz};

                    // Leaf color based on local dome orientation & micro-leaf noise
                    float leafShade = (-dy * 0.55f - dx * 0.25f) + (rand01(x, y, clusterSeed + 3u) - 0.5f) * 0.42f;
                    RGBA8 col = colMid;
                    if (leafShade > 0.16f) {
                        col = colLit;
                    } else if (leafShade < -0.18f || rNorm > 0.86f) {
                        col = colDeep;
                    }
                    // Subtle warm-cool variation per leaf
                    int jitterG = static_cast<int>((rand01(x, y, clusterSeed + 5u) - 0.5f) * 18.0f);
                    col.g = static_cast<uint8_t>(std::clamp(static_cast<int>(col.g) + jitterG, 15, 250));

                    float hVal = zBias + nz * 0.28f;
                    int idx = y * kS + x;
                    if (s.albedo[idx].a < 128 || hVal >= s.height[idx]) {
                        s.setPixelNormal(x, y, col, norm, hVal);
                    }
                }
            }
        }
    };

    if (style == 0) {
        // Style 0: Realms 2.5D Deciduous Oak with gnarled root trunk & volumetric boughs
        // 1. Gnarled bark trunk & root flare in lower center (partially shadowed under canopy)
        for (int y = 38; y <= 61; ++y) {
            float t = static_cast<float>(y - 38) / 23.0f;
            float flare = t * t * 4.5f;
            int hw = static_cast<int>(3.2f + flare);
            for (int x = 32 - hw; x <= 32 + hw; ++x) {
                float dx = static_cast<float>(x - 32) / std::max(1.0f, static_cast<float>(hw));
                float barkGrain = (rand01(x, y, seed + 77u) - 0.5f) * 18.0f;
                float shade = (1.0f - t * 0.25f) * (dx < 0.0f ? 1.05f : 0.72f);
                uint8_t r = static_cast<uint8_t>(std::clamp(96.0f * shade + barkGrain, 28.0f, 150.0f));
                uint8_t g = static_cast<uint8_t>(std::clamp(66.0f * shade + barkGrain * 0.7f, 20.0f, 110.0f));
                uint8_t b = static_cast<uint8_t>(std::clamp(44.0f * shade + barkGrain * 0.5f, 14.0f, 80.0f));
                s.setPixelNormal(x, y, RGBA8{r, g, b, 255}, {dx * 0.7f, -0.15f, 0.7f}, 0.45f);
            }
        }

        constexpr RGBA8 kOakDeep  { 24,  62,  28, 255};
        constexpr RGBA8 kOakShade { 38,  96,  36, 255};
        constexpr RGBA8 kOakMid   { 66, 142,  44, 255};
        constexpr RGBA8 kOakLit   {118, 188,  54, 255};
        constexpr RGBA8 kOakSun   {154, 214,  68, 255};

        // Back shadow boughs
        paintLeafCluster(18.0f, 39.0f, 12.5f, 10.5f, kOakMid, kOakShade, kOakDeep, 0.50f, seed + 1u);
        paintLeafCluster(46.0f, 39.0f, 12.5f, 10.5f, kOakMid, kOakShade, kOakDeep, 0.50f, seed + 2u);
        paintLeafCluster(32.0f, 42.0f, 14.0f, 10.0f, kOakMid, kOakShade, kOakDeep, 0.52f, seed + 3u);

        // Mid-canopy lush volume lobes
        paintLeafCluster(19.0f, 28.0f, 13.0f, 11.5f, kOakLit, kOakMid, kOakShade, 0.65f, seed + 4u);
        paintLeafCluster(45.0f, 29.0f, 12.5f, 11.0f, kOakLit, kOakMid, kOakShade, 0.64f, seed + 5u);
        paintLeafCluster(32.0f, 30.0f, 14.5f, 12.0f, kOakLit, kOakMid, kOakShade, 0.72f, seed + 6u);

        // Sun-drenched crown clusters
        paintLeafCluster(25.0f, 18.0f, 11.5f, 10.0f, kOakSun, kOakLit, kOakMid, 0.82f, seed + 7u);
        paintLeafCluster(39.0f, 19.0f, 11.0f,  9.5f, kOakSun, kOakLit, kOakMid, 0.80f, seed + 8u);
        paintLeafCluster(32.0f, 14.0f, 12.0f, 10.0f, kOakSun, kOakLit, kOakMid, 0.88f, seed + 9u);

        // Base grass tuft wrapping the roots
        paintLeafCluster(32.0f, 59.0f, 11.0f,  3.5f, kOakLit, kOakMid, kOakDeep, 0.58f, seed + 10u);

    } else if (style == 1) {
        // Style 1: Realms 2.5D Layered Boreal Pine / Spruce Cluster
        constexpr RGBA8 kPineDeep { 16,  46,  32, 255};
        constexpr RGBA8 kPineShade{ 26,  76,  48, 255};
        constexpr RGBA8 kPineMid  { 44, 118,  66, 255};
        constexpr RGBA8 kPineLit  { 82, 164,  84, 255};

        auto drawPineBoughTier = [&](float cx, int topY, int botY, float maxW, float zBase, uint32_t tSeed) {
            for (int y = topY; y <= botY; ++y) {
                float t = static_cast<float>(y - topY) / std::max(1.0f, static_cast<float>(botY - topY));
                float branchScallop = 0.78f + 0.22f * std::sin(t * 9.42f + static_cast<float>(tSeed & 3u));
                float hw = (1.5f + t * maxW) * branchScallop;
                int x0 = std::max(1, static_cast<int>(cx - hw));
                int x1 = std::min(kS - 2, static_cast<int>(cx + hw));
                for (int x = x0; x <= x1; ++x) {
                    float dx = (static_cast<float>(x) - cx) / std::max(1.0f, hw);
                    // Hanging needle tufts at bottom of tier
                    float needleDrop = std::sin(dx * 18.0f + static_cast<float>(tSeed)) * 0.12f;
                    if (t > 0.88f + needleDrop && std::abs(dx) > 0.2f) continue;

                    float nNoise = rand01(x, y, tSeed + static_cast<uint32_t>(y));
                    Vec3f norm{dx * 0.70f, 0.45f - t * 0.55f, std::sqrt(std::max(0.1f, 1.0f - dx * dx * 0.5f))};
                    RGBA8 col = kPineMid;
                    if (dx < -0.15f && t < 0.65f) col = kPineLit;
                    else if (dx > 0.20f || t > 0.78f) col = kPineShade;
                    if (t > 0.84f || nNoise < 0.18f) col = kPineDeep;

                    s.setPixelNormal(x, y, col, norm, zBase + (1.0f - std::abs(dx)) * 0.22f);
                }
            }
        };

        // Secondary background pine
        drawPineBoughTier(43.0f, 36, 59, 14.5f, 0.50f, seed + 21u);
        drawPineBoughTier(43.0f, 24, 42, 11.5f, 0.56f, seed + 22u);
        drawPineBoughTier(43.0f, 13, 28,  8.5f, 0.62f, seed + 23u);

        // Primary foreground tall pine
        drawPineBoughTier(24.0f, 34, 60, 17.5f, 0.65f, seed + 11u);
        drawPineBoughTier(24.0f, 21, 40, 14.0f, 0.72f, seed + 12u);
        drawPineBoughTier(24.0f, 10, 26, 10.5f, 0.78f, seed + 13u);
        drawPineBoughTier(24.0f,  4, 15,  6.5f, 0.85f, seed + 14u);

    } else if (style == 2) {
        // Style 2: Coastal Palm & Tropical Shrub
        constexpr RGBA8 kTropLit  {136, 208,  64, 255};
        constexpr RGBA8 kTropMid  { 68, 156,  48, 255};
        constexpr RGBA8 kTropDeep { 28,  86,  36, 255};

        paintLeafCluster(32.0f, 49.0f, 14.0f, 9.0f, kTropLit, kTropMid, kTropDeep, 0.60f, seed + 30u);
        paintLeafCluster(21.0f, 46.0f, 11.0f, 8.0f, kTropLit, kTropMid, kTropDeep, 0.66f, seed + 31u);
        paintLeafCluster(43.0f, 46.0f, 11.0f, 8.0f, kTropLit, kTropMid, kTropDeep, 0.66f, seed + 32u);
        paintLeafCluster(32.0f, 28.0f, 15.0f, 11.0f, kTropLit, kTropMid, kTropDeep, 0.78f, seed + 33u);

    } else {
        // Style 3: Realms-style 2.5D Grass Blades & Wildflower Tuft (StandeeGrassPatch)
        // Generates ~26 sweeping curved grass blades with per-blade 3D normals and sunlit tips
        for (int b = 0; b < 28; ++b) {
            uint32_t bSeed = seed + static_cast<uint32_t>(b * 31);
            float baseX = 10.0f + rand01(b, 0, bSeed) * 44.0f;
            float baseY = 56.0f + rand01(b, 1, bSeed) * 5.0f;
            float bladeH = 18.0f + rand01(b, 2, bSeed) * 24.0f;
            float leanX  = (baseX - 32.0f) * 0.42f + (rand01(b, 3, bSeed) - 0.5f) * 12.0f;

            int steps = static_cast<int>(bladeH * 1.6f);
            for (int st = 0; st <= steps; ++st) {
                float t = static_cast<float>(st) / static_cast<float>(steps); // 0 = root, 1 = tip
                float bx = baseX + leanX * (t * t);
                float by = baseY - bladeH * t;
                float halfW = (1.0f - t * 0.78f) * 1.85f;

                // Smooth gradient from deep mossy AO at root to vibrant sunlit emerald/chartreuse at tip
                uint8_t r = static_cast<uint8_t>(std::clamp(28.0f + t * 98.0f, 20.0f, 160.0f));
                uint8_t g = static_cast<uint8_t>(std::clamp(74.0f + t * 134.0f, 50.0f, 230.0f));
                uint8_t bl = static_cast<uint8_t>(std::clamp(24.0f + t * 34.0f, 18.0f, 80.0f));

                Vec3f bladeNorm{leanX * 0.04f, 0.35f + t * 0.55f, 0.78f};
                for (float dx = -halfW; dx <= halfW; dx += 0.65f) {
                    s.setPixelNormal(static_cast<int>(std::round(bx + dx)),
                                     static_cast<int>(std::round(by)),
                                     RGBA8{r, g, bl, 255},
                                     bladeNorm,
                                     0.50f + t * 0.35f);
                }
            }
            // Occasional tiny sunlit wildflower / clover blossom at blade tip
            if (b % 7 == 0) {
                float fx = baseX + leanX;
                float fy = baseY - bladeH;
                RGBA8 flCol = (b % 14 == 0) ? RGBA8{252, 232, 92, 255} : RGBA8{245, 248, 252, 255};
                s.fillCircle(fx, fy, 1.8f, flCol, 0.90f);
            }
        }
    }
}

// Synthesizes 2.5D Tabletop Settlement Icons (Keep, Hamlet, Windmill, Lighthouse, Monastery, Harbor)
void synthSettlementStandee(LayerSlice& s, int kind) {
    s.clear();
    RGBA8 wallLit   {248, 248, 252, 255};
    RGBA8 wallShade {205, 214, 226, 255};
    RGBA8 roofRed   {214,  78,  52, 255};
    RGBA8 roofSlate { 76,  96, 122, 255};
    RGBA8 goldGlow  {255, 212,  88, 255};

    auto drawGabledBox = [&](int x0, int x1, int roofTopY, int eaveY, int baseY, RGBA8 roofCol) {
        int midX = (x0 + x1) / 2;
        // Walls
        for (int y = eaveY; y <= baseY; ++y) {
            for (int x = x0; x <= x1; ++x) {
                RGBA8 c = (x < midX) ? wallLit : wallShade;
                if (x >= midX && ((x + y) % 4 == 0)) c = kInkMid;
                s.setPixel(x, y, c, 0.65f);
            }
        }
        // Pitched Roof
        for (int y = roofTopY; y <= eaveY; ++y) {
            float t = static_cast<float>(y - roofTopY) / std::max(1.0f, static_cast<float>(eaveY - roofTopY));
            int hw = static_cast<int>(t * ((x1 - x0) / 2 + 3));
            for (int x = midX - hw; x <= midX + hw; ++x) {
                RGBA8 c = roofCol;
                if (y % 3 == 0) c = RGBA8{static_cast<uint8_t>(c.r * 0.8f), static_cast<uint8_t>(c.g * 0.8f), static_cast<uint8_t>(c.b * 0.8f), 255};
                s.setPixel(x, y, c, 0.8f);
            }
        }
        // Glowing arched window
        s.fillRect(midX - 2, (eaveY + baseY) / 2 - 2, midX + 2, (eaveY + baseY) / 2 + 2, goldGlow, 0.7f, 0.95f);
    };

    if (kind == 0) {
        // Citadel Keep / Lowmillburg Castle: Twin crenellated towers + central keep + crimson banner
        s.fillRect(10, 26, 22, 60, wallShade, 0.6f);
        s.fillRect(42, 26, 54, 60, wallShade, 0.6f);
        drawGabledBox(20, 44, 10, 24, 60, roofRed);
        // Crenellations
        for (int x = 10; x <= 54; x += 4) {
            s.fillRect(x, 22, x + 2, 26, wallLit, 0.75f);
        }
        // Banner pennant at peak
        s.drawLine(32.0f, 3.0f, 32.0f, 11.0f, 1.0f, kInkDark, 0.8f);
        s.fillRect(33, 4, 42, 8, roofRed, 0.8f);
    } else if (kind == 1) {
        // Hamlet Cottage Cluster (Coldheartfield / Oakenford)
        drawGabledBox(10, 32, 22, 34, 60, roofRed);
        drawGabledBox(26, 52, 16, 30, 60, roofRed);
    } else if (kind == 2) {
        // Windmill with 4 lattice sails
        drawGabledBox(22, 42, 18, 30, 60, roofRed);
        // Crossed windmill sails
        s.drawLine(10.0f, 10.0f, 54.0f, 44.0f, 2.0f, RGBA8{250, 248, 240, 255}, 0.9f);
        s.drawLine(54.0f, 10.0f, 10.0f, 44.0f, 2.0f, RGBA8{250, 248, 240, 255}, 0.9f);
        s.drawLine(32.0f, 8.0f, 32.0f, 46.0f, 1.6f, RGBA8{160, 110, 70, 255}, 0.9f);
    } else if (kind == 3) {
        // Coastal Lighthouse (Stormpool / Beacon Isle)
        for (int y = 14; y <= 60; ++y) {
            float t = static_cast<float>(y - 14) / 46.0f;
            int hw = 5 + static_cast<int>(t * 6.0f);
            bool stripe = ((y / 7) % 2 == 0);
            RGBA8 c = stripe ? wallLit : roofRed;
            s.fillRect(32 - hw, y, 32 + hw, y, c, 0.75f);
        }
        s.fillCircle(32.0f, 12.0f, 5.5f, goldGlow, 0.95f, 1.0f);
        drawGabledBox(24, 40, 4, 9, 10, roofSlate);
    } else if (kind == 4) {
        // Alpine Monastery Spire
        drawGabledBox(18, 46, 22, 36, 60, roofSlate);
        drawGabledBox(26, 38,  5, 22, 36, roofSlate);
    } else {
        // Port Harbor Pier & Warehouse (Stormpool)
        drawGabledBox(14, 44, 20, 32, 56, roofRed);
        s.fillRect(8, 54, 56, 60, RGBA8{132, 96, 62, 255}, 0.5f);
    }

    s.drawLine(6.0f, 60.0f, 58.0f, 60.0f, 1.2f, kInkDark, 0.4f);
    s.applyInkOutlineAndPaperCutout();
}

#include "hero_sprite_data.inl"
#include "ship_sprite_data.inl"

// Synthesizes Cat Quest Hero, Companion, Sailing Caravel, Monsters & Dungeon Portals
void synthCharacterOrPropStandee(LayerSlice& s, uint16_t layer) {
    s.clear();
    if (layer == InkLayer::StandeeHeroCat) {
        // 16-bit Pixel Art Battle-Mage Adventurer (Blue Hooded Cloak, Steel Pauldrons, Wizard Staff with Radiant Orb)
        for (int y = 0; y < kS; ++y) {
            for (int x = 0; x < kS; ++x) {
                uint32_t p = kHeroSpriteData[y * kS + x];
                uint8_t a = static_cast<uint8_t>((p >> 24) & 0xFF);
                if (a < 64) continue;
                uint8_t r = static_cast<uint8_t>((p >> 16) & 0xFF);
                uint8_t g = static_cast<uint8_t>((p >>  8) & 0xFF);
                uint8_t b = static_cast<uint8_t>( p        & 0xFF);
                float h = 0.5f;
                float em = 0.0f;
                // Radiant crystal orb on wooden staff emits light
                if (r > 200 && g > 220 && b > 230 && y < 18) {
                    h = 0.95f;
                    em = 0.95f;
                } else if (r > 195 && g > 195 && b > 195 && x >= 36 && x <= 46 && y >= 14 && y <= 24) {
                    h = 0.85f; // Polished steel pauldron glint
                } else if (y < 24) {
                    h = 0.72f; // Hair & face
                }
                s.setPixel(x, y, RGBA8{r, g, b, a}, h, em);
            }
        }
    } else if (layer == InkLayer::StandeeHeroCompanion) {
        // Floating Arcane Cartographer Spirit Orb / Owl-Mage
        s.fillCircle(32.0f, 30.0f, 9.0f, RGBA8{110, 205, 255, 255}, 0.9f, 0.85f);
        s.fillCircle(32.0f, 30.0f, 5.0f, RGBA8{245, 250, 255, 255}, 0.95f, 1.0f);
    } else if (layer == InkLayer::StandeeShipCaravel) {
        // High-Fidelity Hand-Inked Antique Caravel Flagship
        // Billowing parchment sails, heraldic cross, quarterdeck cat captain, glowing stern lantern & submerged waterline keel
        for (int y = 0; y < kS; ++y) {
            for (int x = 0; x < kS; ++x) {
                uint32_t p = kShipSpriteData[y * kS + x];
                uint8_t a = static_cast<uint8_t>((p >> 24) & 0xFF);
                if (a < 32) continue;
                uint8_t r = static_cast<uint8_t>((p >> 16) & 0xFF);
                uint8_t g = static_cast<uint8_t>((p >>  8) & 0xFF);
                uint8_t b = static_cast<uint8_t>( p        & 0xFF);
                float h = 0.55f;
                float em = 0.0f;

                // 1. Radiant Aft Stern Lantern (Warm amber/gold light casts beacon across water)
                if (x >= 7 && x <= 11 && y >= 39 && y <= 42 && r > 240 && g > 160) {
                    h = 0.95f;
                    em = 1.0f;
                }
                // 2. Billowing Canvas Sails (3D curved dome relief so sunlight catches wind belly)
                else if (y >= 20 && y <= 42 && (r > 170 && g > 150 && b > 120)) {
                    float sailDome = 1.0f - std::abs((x - 32.0f) / 14.0f);
                    h = 0.70f + std::clamp(sailDome, 0.0f, 1.0f) * 0.22f;
                }
                // 3. Golden Cat Figurehead & Brass Rivets
                else if (r > 200 && g > 150 && b < 100 && y >= 42 && y <= 48) {
                    h = 0.82f;
                }
                // 4. Wooden Hull Planking & Keel
                else if (y >= 44) {
                    h = (y >= 57) ? 0.45f : 0.62f;
                }

                s.setPixel(x, y, RGBA8{r, g, b, a}, h, em);
            }
        }
    } else if (layer == InkLayer::StandeeDungeonArch) {
        // Skull Dungeon Fort Emblem (like Fort Of The Undead-One & Shattered Palace in reference!)
        // Outer stone archway
        s.fillCircle(32.0f, 38.0f, 19.0f, RGBA8{228, 232, 238, 255}, 0.7f);
        s.fillRect(14, 38, 50, 60, RGBA8{205, 214, 224, 255}, 0.7f);
        // Inked Skull Emblem in center
        s.fillCircle(32.0f, 32.0f, 10.5f, RGBA8{252, 252, 255, 255}, 0.85f);
        s.fillRect(26, 38, 38, 48, RGBA8{252, 252, 255, 255}, 0.85f);
        // Dark eye sockets
        s.fillCircle(28.5f, 33.0f, 2.6f, kInkDark, 0.95f);
        s.fillCircle(35.5f, 33.0f, 2.6f, kInkDark, 0.95f);
        // Nasal cavity & jaw teeth
        s.fillRect(31, 37, 33, 40, kInkDark, 0.95f);
        s.drawLine(27.0f, 44.0f, 37.0f, 44.0f, 1.2f, kInkDark, 0.95f);
        for (int tx = 28; tx <= 36; tx += 2) {
            s.drawLine(static_cast<float>(tx), 42.0f, static_cast<float>(tx), 46.0f, 0.9f, kInkDark, 0.9f);
        }
        // Crossed bones below
        s.drawLine(18.0f, 55.0f, 46.0f, 55.0f, 2.4f, RGBA8{248, 248, 252, 255}, 0.85f);
    } else if (layer == InkLayer::StandeeMonsterSlime) {
        s.fillCircle(32.0f, 45.0f, 13.0f, RGBA8{82, 188, 142, 255}, 0.8f, 0.2f);
        s.fillRect(26, 41, 29, 45, kInkDark, 0.9f);
        s.fillRect(35, 41, 38, 45, kInkDark, 0.9f);
    } else if (layer == InkLayer::StandeeMonsterBandit) {
        s.fillRect(22, 32, 42, 58, RGBA8{142, 64, 54, 255}, 0.75f);
        s.fillCircle(32.0f, 23.0f, 9.5f, RGBA8{198, 152, 118, 255}, 0.85f);
        s.drawLine(42.0f, 42.0f, 54.0f, 22.0f, 2.0f, RGBA8{180, 185, 195, 255}, 0.85f);
    } else if (layer == InkLayer::StandeeMonsterWyvern) {
        // Elder Sea-Wyvern / Dragon Boss Standee
        s.drawLine(10.0f, 16.0f, 32.0f, 34.0f, 5.5f, RGBA8{176, 56, 48, 255}, 0.8f);
        s.drawLine(54.0f, 16.0f, 32.0f, 34.0f, 5.5f, RGBA8{176, 56, 48, 255}, 0.8f);
        s.fillCircle(32.0f, 38.0f, 13.0f, RGBA8{148, 42, 38, 255}, 0.85f);
        s.fillCircle(32.0f, 18.0f,  9.0f, RGBA8{196, 68, 52, 255}, 0.9f);
        s.fillCircle(29.0f, 16.0f,  2.0f, RGBA8{255, 220, 80, 255}, 0.95f, 1.0f);
        s.fillCircle(35.0f, 16.0f,  2.0f, RGBA8{255, 220, 80, 255}, 0.95f, 1.0f);
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
        s.fillRect(24, 14, 40, 60, RGBA8{138, 142, 148, 255}, 0.8f);
        s.fillCircle(32.0f, 30.0f, 4.5f, RGBA8{90, 210, 255, 255}, 0.95f, 1.0f);
    } else if (layer == InkLayer::StandeeTreasureChest) {
        s.fillRect(16, 34, 48, 58, RGBA8{156, 98, 48, 255}, 0.75f);
        s.fillRect(16, 43, 48, 47, RGBA8{242, 198, 72, 255}, 0.9f, 0.5f);
    } else if (layer == InkLayer::StandeeCampfire) {
        s.drawLine(18.0f, 56.0f, 46.0f, 48.0f, 2.5f, RGBA8{112, 72, 42, 255}, 0.5f);
        s.drawLine(18.0f, 48.0f, 46.0f, 56.0f, 2.5f, RGBA8{112, 72, 42, 255}, 0.5f);
        s.fillCircle(32.0f, 40.0f, 9.0f, RGBA8{255, 165, 48, 255}, 0.9f, 1.0f);
        s.fillCircle(32.0f, 42.0f, 5.0f, RGBA8{255, 238, 128, 255}, 0.95f, 1.0f);
    } else {
        // Sunken Ruin / Sea Stack / Signpost / Marsh Reeds fallback
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
