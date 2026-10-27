#include "eatscharted/ink_canvas.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

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

    void clear(RGBA8 c = {0, 0, 0, 0}, float h = 0.0f) noexcept {
        for (int i = 0; i < kS * kS; ++i) {
            albedo[i] = c;
            height[i] = h;
            emissive[i] = 0.0f;
        }
    }

    void setPixel(int x, int y, RGBA8 c, float h = 0.5f, float em = 0.0f) noexcept {
        if (x < 0 || x >= kS || y < 0 || y >= kS) return;
        int idx = y * kS + x;
        albedo[idx] = c;
        height[idx] = h;
        emissive[idx] = em;
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
                    float domeH = h + std::sqrt(std::max(0.0f, 1.0f - d2)) * 0.35f;
                    setPixel(x, y, fillCol, domeH, em);
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
        RGBA8 copy[kS * kS];
        std::memcpy(copy, albedo, sizeof(copy));

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

                auto hAt = [&](int sx, int sy) -> float {
                    sx = std::clamp(sx, 0, kS - 1);
                    sy = std::clamp(sy, 0, kS - 1);
                    return height[sy * kS + sx];
                };

                float dX = (hAt(x + 1, y) - hAt(x - 1, y)) * 1.8f;
                float dY = (hAt(x, y + 1) - hAt(x, y - 1)) * 1.8f;
                float nx = -dX;
                float ny = -dY;
                float nz = 1.0f;
                float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (len > 1e-5f) { nx /= len; ny /= len; nz /= len; }

                out.normalBytes[dst + 0] = static_cast<uint8_t>(std::clamp((nx * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                out.normalBytes[dst + 1] = static_cast<uint8_t>(std::clamp((ny * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                out.normalBytes[dst + 2] = static_cast<uint8_t>(std::clamp((nz * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                out.normalBytes[dst + 3] = static_cast<uint8_t>(std::clamp(emissive[idx] * 255.0f, 0.0f, 255.0f));
            }
        }
    }
};

// Watabou-style hachured mountain standee generator
void synthMountainStandee(LayerSlice& s, uint32_t seed, bool isDualRidge) {
    s.clear();
    int peakX = isDualRidge ? 24 : 32;
    int peakY = isDualRidge ? 12 : 7;
    int baseLeft = 4;
    int baseRight = 60;
    int baseY = 61;

    for (int y = peakY; y <= baseY; ++y) {
        float t = static_cast<float>(y - peakY) / static_cast<float>(baseY - peakY);
        int leftX  = static_cast<int>(peakX - t * (peakX - baseLeft)  + std::sin(y * 0.35f) * 1.8f);
        int rightX = static_cast<int>(peakX + t * (baseRight - peakX) + std::cos(y * 0.28f) * 1.8f);
        int ridgeX = peakX + static_cast<int>(std::sin(y * 0.22f + static_cast<float>(seed & 7u)) * 3.2f);

        for (int x = leftX; x <= rightX; ++x) {
            bool isSunlitSide = (x < ridgeX);
            RGBA8 col = isSunlitSide ? kPaperCream : kPaperShadow;
            // Snow-tinted parchment cap near summit
            if (t < 0.28f && isSunlitSide) {
                col = RGBA8{250, 245, 232, 255};
            }
            // Watabou diagonal ink hachure strokes on the right shaded slope!
            if (!isSunlitSide) {
                int hachurePattern = (x + y * 2) % 5;
                if (hachurePattern == 0 || (x - ridgeX < 5 && hachurePattern == 2)) {
                    col = kInkMid;
                }
            } else {
                // Sparse contour stipple on sunlit side
                if (((x * 3 - y) % 13 == 0) && (x > ridgeX - 6)) {
                    col = kPaperWarm;
                }
            }
            float h = 0.5f + (1.0f - std::abs(static_cast<float>(x - ridgeX)) / 28.0f) * 0.45f;
            s.setPixel(x, y, col, h);
        }
        // Central crooked ridge spine in bold India ink
        s.setPixel(ridgeX, y, kInkDark, 0.95f);
        s.setPixel(ridgeX + 1, y, kInkDark, 0.95f);
    }

    if (isDualRidge) {
        int p2X = 44, p2Y = 18;
        for (int y = p2Y; y <= baseY; ++y) {
            float t = static_cast<float>(y - p2Y) / static_cast<float>(baseY - p2Y);
            int lx = static_cast<int>(p2X - t * 18.0f);
            int rx = static_cast<int>(p2X + t * 15.0f);
            for (int x = lx; x <= rx; ++x) {
                bool sunlit = (x < p2X);
                RGBA8 col = sunlit ? kPaperWarm : kPaperShadow;
                if (!sunlit && ((x + y * 2) % 4 == 0)) col = kInkMid;
                s.setPixel(x, y, col, 0.65f);
            }
            s.setPixel(p2X, y, kInkDark, 0.9f);
        }
    }

    // Ink baseline ticks (Watabou baseline grounding strokes)
    s.drawLine(6.0f, 61.0f, 58.0f, 61.0f, 1.2f, kInkDark, 0.4f);
    s.applyInkOutlineAndPaperCutout();
}

// Watabou-style clumped ink-wash tree canopy standee
void synthForestCanopy(LayerSlice& s, uint32_t seed, int style) {
    s.clear();
    // 0 = Deciduous Oak Cluster, 1 = Boreal Pine Cluster, 2 = Coastal Palm Cluster
    RGBA8 litWash   = (style == 0) ? RGBA8{138, 168, 102, 255} :
                      (style == 1) ? RGBA8{ 92, 134, 108, 255} :
                                     RGBA8{148, 178,  96, 255};
    RGBA8 shadeWash = (style == 0) ? RGBA8{ 94, 122,  68, 255} :
                      (style == 1) ? RGBA8{ 60,  94,  76, 255} :
                                     RGBA8{102, 134,  64, 255};

    // Draw dark inked trunks with vertical hachures at bottom
    if (style != 2) {
        s.fillRect(20, 44, 24, 60, RGBA8{86, 62, 44, 255}, 0.4f);
        s.fillRect(38, 46, 42, 60, RGBA8{86, 62, 44, 255}, 0.4f);
        s.fillRect(29, 42, 34, 61, RGBA8{72, 50, 36, 255}, 0.45f);
    }

    if (style == 0) {
        // Puffy clumped Watabou canopy lobes
        s.fillCircle(21.0f, 34.0f, 13.0f, shadeWash, 0.65f);
        s.fillCircle(43.0f, 35.0f, 12.5f, shadeWash, 0.65f);
        s.fillCircle(32.0f, 23.0f, 15.0f, litWash,   0.85f);
        s.fillCircle(22.0f, 29.0f, 11.0f, litWash,   0.80f);
        s.fillCircle(40.0f, 30.0f, 10.5f, litWash,   0.78f);

        // Interior Watabou leaf stipple & underside cross-hatching
        for (int y = 10; y < 50; ++y) {
            for (int x = 8; x < 56; ++x) {
                if (s.getPixel(x, y).a > 128) {
                    if (y > 36 && ((x + y) % 3 == 0)) {
                        s.setPixel(x, y, kInkMid, 0.5f);
                    } else if (rand01(x, y, seed) < 0.12f) {
                        s.setPixel(x, y, shadeWash, 0.7f);
                    }
                }
            }
        }
    } else if (style == 1) {
        // Tiered coniferous pine spires (twin trees)
        auto drawPine = [&](int cx, int topY, int botY, float maxW) {
            for (int y = topY; y <= botY; ++y) {
                float t = static_cast<float>(y - topY) / static_cast<float>(botY - topY);
                float tierSaw = 0.65f + 0.35f * std::fmod(t * 4.0f, 1.0f);
                int hw = static_cast<int>(t * maxW * tierSaw);
                for (int x = cx - hw; x <= cx + hw; ++x) {
                    RGBA8 col = (x < cx) ? litWash : shadeWash;
                    if (x > cx + 2 && ((x + y) % 3 == 0)) col = kInkMid;
                    s.setPixel(x, y, col, 0.75f);
                }
            }
        };
        drawPine(22, 14, 50, 14.0f);
        drawPine(42, 18, 52, 13.0f);
        drawPine(32,  7, 48, 16.0f);
    } else {
        // Curved tropical coastal palm standee
        s.drawLine(28.0f, 60.0f, 33.0f, 24.0f, 2.5f, RGBA8{122, 90, 58, 255}, 0.55f);
        s.drawLine(33.0f, 24.0f, 14.0f, 18.0f, 2.2f, litWash, 0.75f);
        s.drawLine(33.0f, 24.0f, 50.0f, 19.0f, 2.2f, shadeWash, 0.75f);
        s.drawLine(33.0f, 24.0f, 18.0f, 31.0f, 2.0f, shadeWash, 0.70f);
        s.drawLine(33.0f, 24.0f, 48.0f, 32.0f, 2.0f, litWash, 0.70f);
        s.drawLine(33.0f, 24.0f, 33.0f,  9.0f, 2.2f, litWash, 0.80f);
    }

    s.drawLine(14.0f, 60.0f, 50.0f, 60.0f, 1.1f, kInkDark, 0.3f);
    s.applyInkOutlineAndPaperCutout();
}

// Synthesizes 2.5D Tabletop Settlement Icons (Keep, Hamlet, Windmill, Lighthouse, Monastery, Harbor)
void synthSettlementStandee(LayerSlice& s, int kind) {
    s.clear();
    RGBA8 wallLit   {236, 222, 194, 255};
    RGBA8 wallShade {192, 174, 142, 255};
    RGBA8 roofRed   {188,  84,  62, 255};
    RGBA8 roofSlate { 78,  96, 122, 255};
    RGBA8 goldGlow  {255, 210,  92, 255};

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
        // Citadel Keep: Twin crenellated towers + central high keep + crimson banner
        s.fillRect(10, 26, 22, 60, wallShade, 0.6f);
        s.fillRect(42, 26, 54, 60, wallShade, 0.6f);
        drawGabledBox(20, 44, 10, 24, 60, roofSlate);
        // Crenellations
        for (int x = 10; x <= 54; x += 4) {
            s.fillRect(x, 22, x + 2, 26, wallLit, 0.75f);
        }
        // Banner pennant at peak
        s.drawLine(32.0f, 3.0f, 32.0f, 11.0f, 1.0f, kInkDark, 0.8f);
        s.fillRect(33, 4, 42, 8, roofRed, 0.8f);
    } else if (kind == 1) {
        // Hamlet Cottage Cluster
        drawGabledBox(10, 32, 22, 34, 60, roofRed);
        drawGabledBox(26, 52, 16, 30, 60, roofRed);
    } else if (kind == 2) {
        // Windmill
        drawGabledBox(22, 42, 18, 30, 60, roofRed);
        // Crossed windmill sails
        s.drawLine(10.0f, 10.0f, 54.0f, 44.0f, 1.8f, kPaperCream, 0.9f);
        s.drawLine(54.0f, 10.0f, 10.0f, 44.0f, 1.8f, kPaperCream, 0.9f);
    } else if (kind == 3) {
        // Coastal Lighthouse
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
        // Port Harbor Pier & Warehouse
        drawGabledBox(14, 44, 20, 32, 56, roofSlate);
        s.fillRect(8, 54, 56, 60, RGBA8{132, 96, 62, 255}, 0.5f);
    }

    s.drawLine(6.0f, 60.0f, 58.0f, 60.0f, 1.2f, kInkDark, 0.4f);
    s.applyInkOutlineAndPaperCutout();
}

// Synthesizes Cat Quest Hero, Companion, Sailing Caravel, Monsters & Dungeon Portals
void synthCharacterOrPropStandee(LayerSlice& s, uint16_t layer) {
    s.clear();
    if (layer == InkLayer::StandeeHeroCat) {
        // Cat Quest Armored Feline Adventurer!
        // Fluttering crimson cape
        s.fillCircle(26.0f, 44.0f, 12.0f, RGBA8{196, 48, 48, 255}, 0.55f);
        // Cobalt steel knight armor torso
        s.fillRect(24, 34, 40, 54, RGBA8{68, 108, 168, 255}, 0.75f);
        s.fillRect(27, 36, 37, 48, RGBA8{210, 220, 235, 255}, 0.85f); // Chestplate shine
        // Golden-tabby cat head & pointed ears!
        s.fillCircle(32.0f, 24.0f, 10.5f, RGBA8{238, 176, 78, 255}, 0.85f);
        // Left & right pointed cat ears
        for (int y = 8; y <= 18; ++y) {
            int w = (y - 8) / 2 + 1;
            s.fillRect(23 - w, y, 23 + w, y, RGBA8{238, 176, 78, 255}, 0.85f);
            s.fillRect(41 - w, y, 41 + w, y, RGBA8{238, 176, 78, 255}, 0.85f);
        }
        // Expressive eyes
        s.fillRect(27, 22, 29, 25, kInkDark, 0.9f);
        s.fillRect(35, 22, 37, 25, kInkDark, 0.9f);
        // Broadsword held aloft on right
        s.drawLine(44.0f, 42.0f, 54.0f, 14.0f, 2.2f, RGBA8{235, 242, 250, 255}, 0.9f, 0.25f);
        s.drawLine(40.0f, 38.0f, 48.0f, 44.0f, 1.8f, RGBA8{225, 180, 64, 255}, 0.85f);
        // Boots
        s.fillRect(25, 54, 30, 60, kInkDark, 0.6f);
        s.fillRect(34, 54, 39, 60, kInkDark, 0.6f);
    } else if (layer == InkLayer::StandeeHeroCompanion) {
        // Floating Arcane Cartographer Spirit Orb / Owl-Mage
        s.fillCircle(32.0f, 30.0f, 9.0f, RGBA8{110, 205, 255, 255}, 0.9f, 0.85f);
        s.fillCircle(32.0f, 30.0f, 5.0f, RGBA8{245, 250, 255, 255}, 0.95f, 1.0f);
    } else if (layer == InkLayer::StandeeShipCaravel) {
        // Hand-inked Nautical Caravel Ship with Billowing Sails
        // Wooden hull
        for (int y = 42; y <= 58; ++y) {
            int shrink = (y - 42) / 2;
            s.fillRect(12 + shrink, y, 52 - shrink, y, RGBA8{134, 88, 52, 255}, 0.7f);
        }
        // Mast
        s.fillRect(31, 8, 33, 44, kInkDark, 0.6f);
        // Billowing main parchment sail with red paw/cross emblem
        s.fillCircle(34.0f, 26.0f, 14.0f, kPaperCream, 0.88f);
        s.fillRect(32, 18, 36, 34, RGBA8{196, 52, 48, 255}, 0.9f);
        s.fillRect(26, 24, 42, 28, RGBA8{196, 52, 48, 255}, 0.9f);
        // Pennant flag at top
        s.fillRect(33, 8, 46, 12, RGBA8{196, 52, 48, 255}, 0.85f);
    } else if (layer == InkLayer::StandeeDungeonArch) {
        // Cave Maw / Stone Dungeon Archway with Glowing Runes
        s.fillCircle(32.0f, 38.0f, 20.0f, RGBA8{128, 122, 114, 255}, 0.7f);
        s.fillRect(12, 38, 52, 60, RGBA8{128, 122, 114, 255}, 0.7f);
        // Dark portal interior with arcane violet/cyan glow
        s.fillCircle(32.0f, 42.0f, 12.0f, RGBA8{38, 24, 58, 255}, 0.3f, 0.4f);
        s.fillCircle(32.0f, 42.0f,  7.0f, RGBA8{92, 185, 245, 255}, 0.4f, 0.9f);
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

    for (uint32_t layer = 0; layer < InkLayer::kCount; ++layer) {
        LayerSlice slice{};
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

            case InkLayer::StandeeMountainPeak:
                synthMountainStandee(slice, seed + 1u, false);
                break;
            case InkLayer::StandeeMountainRidge:
            case InkLayer::StandeeSeaStack:
                synthMountainStandee(slice, seed + 2u, true);
                break;

            case InkLayer::StandeeForestOak:
                synthForestCanopy(slice, seed + 10u, 0);
                break;
            case InkLayer::StandeeForestPine:
                synthForestCanopy(slice, seed + 11u, 1);
                break;
            case InkLayer::StandeeForestPalm:
            case InkLayer::StandeeMarshReeds:
                synthForestCanopy(slice, seed + 12u, 2);
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
