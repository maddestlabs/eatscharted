#include "eatscharted/chart_world.hpp"
#include "eatsbits/ui/glyph/vector_font.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace eatscharted {

namespace {

[[nodiscard]] uint32_t hashPos(int x, int z, uint32_t seed) noexcept {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(z) * 668265263u +
                 seed * 1442695041u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

[[nodiscard]] uint32_t packBiomesAndLayer(BiomeId b0, BiomeId b1, uint16_t inkLayer) noexcept {
    return static_cast<uint32_t>(b0) |
           (static_cast<uint32_t>(b1) << 8u) |
           (static_cast<uint32_t>(inkLayer) << 16u);
}

} // namespace

ChartWorld::ChartWorld() {
    auto t0 = std::chrono::high_resolution_clock::now();
    inkAtlas_ = InkCanvas::generateFullAtlas(1492u);
    auto t1 = std::chrono::high_resolution_clock::now();
    atlasGenTimeMs_ = std::chrono::duration<float, std::milli>(t1 - t0).count();
    atlasGenerated_ = true;
}

void ChartWorld::clear() {
    worldVerts_.clear();
    staticStandees_.clear();
    activeStandees_.clear();
    activeTelegraphs_.clear();
    pointLights_.clear();
    customBanners_.clear();
    enemies_.clear();
    floatingNumbers_.clear();
    mapSurfaceTriCount_ = 0;
    bannerGlyphTriCount_ = 0;
}

void ChartWorld::buildPerilousPeninsula(uint32_t seed) {
    overworldReturnSeed_ = seed;
    overworldReturnPreset_ = AtlasPreset::PerilousPeninsula;
    generateCustomChart(seed, AtlasPreset::PerilousPeninsula);
    hero_.pos = {-4.0f, 0.45f, -2.0f};
    hero_.activeRegionName = "PORT PAWVEN";
    hero_.activeRegionSub  = "The Perilous Shores";
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 28.0f, hero_.pos.z - 25.0f};
}

void ChartWorld::buildShatteredArchipelago(uint32_t seed) {
    overworldReturnSeed_ = seed;
    overworldReturnPreset_ = AtlasPreset::ShatteredArchipelago;
    generateCustomChart(seed, AtlasPreset::ShatteredArchipelago);
    hero_.pos = {0.0f, 0.45f, -4.0f};
    hero_.activeRegionName = "CORSAIR'S HAVEN";
    hero_.activeRegionSub  = "The Shattered Sea";
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 30.0f, hero_.pos.z - 26.0f};
}

void ChartWorld::buildHighlandKingdom(uint32_t seed) {
    overworldReturnSeed_ = seed;
    overworldReturnPreset_ = AtlasPreset::HighlandKingdom;
    generateCustomChart(seed, AtlasPreset::HighlandKingdom);
    hero_.pos = {0.0f, 0.65f, 4.0f};
    hero_.activeRegionName = "HIGHGUARD KEEP";
    hero_.activeRegionSub  = "Silvervale Realm";
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 28.0f, hero_.pos.z - 25.0f};
}

void ChartWorld::buildOnePageDungeon(uint32_t seed) {
    generateCustomChart(seed, AtlasPreset::OnePageDungeon);
    hero_.pos = {0.0f, 0.28f, -22.0f};
    hero_.isSailing = false;
    hero_.activeRegionName = "THE SUNKEN SANCTUM";
    hero_.activeRegionSub  = "Watabou One-Page Dungeon";
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 25.0f, hero_.pos.z - 21.0f};
}

void ChartWorld::generateCustomChart(uint32_t seed, AtlasPreset preset) {
    clear();
    voronoi_.generate(seed, preset);
    populateProceduralStandeesAndEnemies();
}

void ChartWorld::addCustomStandee(Vec3f pos, float w, float h, uint16_t layer, uint32_t flags) {
    StandeeInstance st{};
    st.worldPos[0] = pos.x;
    st.worldPos[1] = pos.y;
    st.worldPos[2] = pos.z;
    st.width       = w;
    st.height      = h;
    st.texLayer    = layer;
    st.flags       = flags;
    st.phaseOffset = std::fmod(std::abs(pos.x * 1.37f + pos.z * 2.19f), 6.28f);
    staticStandees_.push_back(st);
}

void ChartWorld::addCustomBanner(const std::string& text, Vec2f p0, Vec2f p1, Vec2f p2, float letterSize) {
    customBanners_.push_back(CurvedMapBanner{text, p0, p1, p2, letterSize, true, false});
}

void ChartWorld::addPointLight(Vec3f pos, float radius, Vec3f color, float intensity) {
    if (pointLights_.size() >= 64) return;
    PointLight pl{};
    pl.position[0] = pos.x;
    pl.position[1] = pos.y;
    pl.position[2] = pos.z;
    pl.radius      = radius;
    pl.color[0]    = color.x;
    pl.color[1]    = color.y;
    pl.color[2]    = color.z;
    pl.intensity   = intensity;
    pointLights_.push_back(pl);
}

void ChartWorld::populateProceduralStandeesAndEnemies() {
    uint32_t seed = voronoi_.seed();

    if (voronoi_.isDungeonMode()) {
        // Populate One-Page Dungeon pillars, braziers, chests, and dungeon guardians
        for (const auto& rm : voronoi_.dungeonRooms()) {
            float cx = 0.5f * (rm.minX + rm.maxX);
            float cz = 0.5f * (rm.minZ + rm.maxZ);
            // Corner torch braziers with warm point lights
            addCustomStandee({rm.minX + 1.8f, 0.25f, rm.minZ + 1.8f}, 1.4f, 1.5f, InkLayer::StandeeCampfire, StandeeFlags::Emissive | StandeeFlags::IdleBob);
            addCustomStandee({rm.maxX - 1.8f, 0.25f, rm.minZ + 1.8f}, 1.4f, 1.5f, InkLayer::StandeeCampfire, StandeeFlags::Emissive | StandeeFlags::IdleBob);
            addPointLight({cx, 2.2f, cz}, 16.0f, {1.0f, 0.72f, 0.36f}, 1.35f);

            if (rm.isBossRoom) {
                EnemyAgent boss{};
                boss.pos = {cx, 0.25f, cz + 2.0f};
                boss.homePos = boss.pos;
                boss.standeeLayer = InkLayer::StandeeMonsterWyvern;
                boss.name = "The Sunken Wyrm";
                boss.width = 4.6f;
                boss.height = 4.6f;
                boss.hp = 180;
                boss.maxHp = 180;
                boss.telegraphRadius = 6.2f;
                boss.telegraphType = TelegraphType::EnemyCircle;
                boss.isBoss = true;
                enemies_.push_back(boss);
            } else if (rm.id >= 2) {
                EnemyAgent guard{};
                guard.pos = {cx, 0.25f, cz};
                guard.homePos = guard.pos;
                guard.standeeLayer = (rm.id % 2 == 0) ? InkLayer::StandeeMonsterBandit : InkLayer::StandeeMonsterSlime;
                guard.name = (rm.id % 2 == 0) ? "Crypt Marauder" : "Rune Slime";
                guard.hp = 55;
                guard.maxHp = 55;
                guard.telegraphRadius = 3.8f;
                guard.telegraphType = (rm.id % 2 == 0) ? TelegraphType::EnemyCone : TelegraphType::EnemyCircle;
                enemies_.push_back(guard);
            }
        }
        for (const auto& p : voronoi_.pois()) {
            uint16_t layer = InkLayer::StandeeDungeonArch;
            if (p.category == PoiCategory::StandingStones) layer = InkLayer::StandeeStandingStone;
            else if (p.category == PoiCategory::SunkenRuin) layer = InkLayer::StandeeTreasureChest;
            if (p.category != PoiCategory::BossLair) {
                addCustomStandee({p.pos.x, 0.25f, p.pos.y}, 2.6f, 2.8f, layer, StandeeFlags::PopUpTilt);
            }
        }
        return;
    }

    // 1. Place 2.5D Settlement & Landmark POI Standees
    for (const auto& p : voronoi_.pois()) {
        ChartFieldSample s = voronoi_.sampleAt(p.pos.x, p.pos.y);
        float y = std::max(0.05f, s.elevation);
        uint16_t layer = InkLayer::StandeeHamletCottage;
        float w = 3.6f, h = 3.8f;

        switch (p.category) {
            case PoiCategory::CitadelKeep:     layer = InkLayer::StandeeTownKeep;      w = 4.8f; h = 5.0f; break;
            case PoiCategory::PortHarbor:      layer = InkLayer::StandeePortHarbor;    w = 4.0f; h = 3.8f; break;
            case PoiCategory::MarketHamlet:    layer = InkLayer::StandeeHamletCottage; w = 3.8f; h = 3.6f; break;
            case PoiCategory::AlpineMonastery: layer = InkLayer::StandeeMonastery;     w = 3.8f; h = 4.6f; break;
            case PoiCategory::DungeonPortal:   layer = InkLayer::StandeeDungeonArch;   w = 3.5f; h = 3.6f; break;
            case PoiCategory::SunkenRuin:      layer = InkLayer::StandeeSunkenRuin;    w = 3.2f; h = 3.4f; break;
            case PoiCategory::Lighthouse:      layer = InkLayer::StandeeLighthouse;    w = 3.2f; h = 4.5f; break;
            case PoiCategory::Windmill:        layer = InkLayer::StandeeWindmill;      w = 3.5f; h = 3.8f; break;
            case PoiCategory::StandingStones:  layer = InkLayer::StandeeStandingStone; w = 2.8f; h = 3.2f; break;
            case PoiCategory::BossLair:        layer = InkLayer::StandeeDungeonArch;   w = 3.8f; h = 4.0f; break;
        }

        addCustomStandee({p.pos.x, y, p.pos.y}, w, h, layer, StandeeFlags::PopUpTilt);
        addPointLight({p.pos.x, y + 2.4f, p.pos.y - 1.0f}, 14.0f, {1.0f, 0.78f, 0.42f}, 1.25f);

        // Spawn overworld boss or guardians near BossLair / Ruins / Dungeons
        if (p.category == PoiCategory::BossLair) {
            EnemyAgent boss{};
            boss.pos = {p.pos.x - 2.5f, y, p.pos.y - 2.5f};
            boss.homePos = boss.pos;
            boss.standeeLayer = InkLayer::StandeeMonsterWyvern;
            boss.name = p.name;
            boss.width = 4.5f;
            boss.height = 4.5f;
            boss.hp = 200;
            boss.maxHp = 200;
            boss.telegraphRadius = 6.0f;
            boss.telegraphType = TelegraphType::EnemyCircle;
            boss.isBoss = true;
            enemies_.push_back(boss);
        } else if (p.category == PoiCategory::DungeonPortal || p.category == PoiCategory::SunkenRuin) {
            EnemyAgent guard{};
            guard.pos = {p.pos.x - 3.0f, y, p.pos.y - 2.0f};
            guard.homePos = guard.pos;
            guard.standeeLayer = InkLayer::StandeeMonsterBandit;
            guard.name = "Shore Marauder";
            guard.hp = 60;
            guard.maxHp = 60;
            guard.telegraphRadius = 3.8f;
            guard.telegraphType = TelegraphType::EnemyCone;
            enemies_.push_back(guard);
        }
    }

    // 2. Scatter Watabou-style Mountain Ridge Standees, Clumped Forests & Sea Stacks across [-64m, +64m]
    for (int gz = -60; gz <= 60; gz += 5) {
        for (int gx = -60; gx <= 60; gx += 5) {
            uint32_t h = hashPos(gx, gz, seed);
            float jx = (static_cast<float>(h & 0xFFu) / 255.0f - 0.5f) * 3.6f;
            float jz = (static_cast<float>((h >> 8u) & 0xFFu) / 255.0f - 0.5f) * 3.6f;
            float wx = static_cast<float>(gx) + jx;
            float wz = static_cast<float>(gz) + jz;

            ChartFieldSample s = voronoi_.sampleAt(wx, wz);
            // Keep roads, rivers, and town plazas clear of clutter
            if (s.sdfRoad < 2.6f || s.sdfRiver < s.riverWidth + 1.4f || (s.flags & ChartFlags::IsSettlement) != 0u) {
                continue;
            }
            // Also keep immediate hero spawn area clear
            if (std::abs(wx + 4.0f) < 5.5f && std::abs(wz + 2.0f) < 5.5f) {
                continue;
            }

            if ((s.flags & ChartFlags::IsWater) != 0u) {
                // Occasional offshore sea stacks in shallow shoals
                if (s.sdfCoast > -9.0f && s.sdfCoast < -2.2f && (h % 19u == 0u)) {
                    addCustomStandee({wx, 0.0f, wz}, 2.6f, 2.8f, InkLayer::StandeeSeaStack, StandeeFlags::PopUpTilt);
                }
                continue;
            }

            if (s.biomePrimary == BiomeId::AlpinePeak || s.elevation > 2.45f) {
                if (h % 3u != 0u) {
                    uint16_t mLayer = ((h >> 4u) & 1u) ? InkLayer::StandeeMountainPeak : InkLayer::StandeeMountainRidge;
                    float scale = 3.8f + static_cast<float>(h & 7u) * 0.25f;
                    addCustomStandee({wx, s.elevation - 0.12f, wz}, scale * 1.15f, scale, mLayer,
                                     StandeeFlags::PopUpTilt | StandeeFlags::PartableCanopy);
                }
            } else if (s.biomePrimary == BiomeId::DeciduousWoodland || s.biomePrimary == BiomeId::LushMeadow) {
                uint32_t mod = (s.biomePrimary == BiomeId::DeciduousWoodland) ? 3u : 9u;
                if (h % mod == 0u && s.sdfCoast > 1.8f) {
                    float scale = 2.6f + static_cast<float>(h & 3u) * 0.28f;
                    addCustomStandee({wx, s.elevation, wz}, scale, scale, InkLayer::StandeeForestOak,
                                     StandeeFlags::WindSway | StandeeFlags::PartableCanopy);
                }
            } else if (s.biomePrimary == BiomeId::TaigaPines) {
                if (h % 4u == 0u) {
                    float scale = 2.7f + static_cast<float>(h & 3u) * 0.25f;
                    addCustomStandee({wx, s.elevation, wz}, scale, scale * 1.1f, InkLayer::StandeeForestPine,
                                     StandeeFlags::WindSway | StandeeFlags::PartableCanopy);
                }
            } else if (s.biomePrimary == BiomeId::SandyBeach && (h % 11u == 0u)) {
                addCustomStandee({wx, s.elevation, wz}, 2.4f, 2.7f, InkLayer::StandeeForestPalm,
                                 StandeeFlags::WindSway | StandeeFlags::PartableCanopy);
            }

            // Scatter wandering slimes in the wilderness for lively Cat Quest overworld combat
            if (s.sdfCoast > 3.5f && s.elevation < 2.0f && (h % 47u == 0u) && enemies_.size() < 14) {
                EnemyAgent slime{};
                slime.pos = {wx, s.elevation, wz};
                slime.homePos = slime.pos;
                slime.standeeLayer = InkLayer::StandeeMonsterSlime;
                slime.name = "Wild Ink Slime";
                slime.width = 1.9f;
                slime.height = 1.9f;
                slime.hp = 40;
                slime.maxHp = 40;
                slime.telegraphRadius = 3.2f;
                slime.telegraphType = TelegraphType::EnemyCircle;
                enemies_.push_back(slime);
            }
        }
    }
}

void ChartWorld::bakeCurvedMapBannersToMesh() {
    // Renders curved calligraphic region titles & POI ribbon labels directly into 3D worldVerts_
    // hovering slightly above the parchment surface so they curve across the 2.5D tabletop!
    auto emitCurvedBanner = [&](const CurvedMapBanner& b) {
        if (b.text.empty()) return;

        // Measure total text width in local 2D space where height = b.letterSize
        float totalW = eatsbits::ui::glyph::getVectorTextWidth(b.letterSize, b.text);
        if (totalW < 0.01f) return;

        // Approximate quadratic Bezier curve arc length
        float chord01 = std::sqrt((b.p1.x - b.p0.x) * (b.p1.x - b.p0.x) + (b.p1.y - b.p0.y) * (b.p1.y - b.p0.y));
        float chord12 = std::sqrt((b.p2.x - b.p1.x) * (b.p2.x - b.p1.x) + (b.p2.y - b.p1.y) * (b.p2.y - b.p1.y));
        float arcLen = std::max(1.0f, chord01 + chord12);

        float tSpan = std::min(0.92f, totalW / arcLen);

        // Generate 2D vector triangles for the string at origin (0, 0) using BatchRenderer2D
        eatsbits::ui::BatchRenderer2D scratch;
        scratch.beginFrame(1000.0f, 1000.0f);
        eatsbits::ui::glyph::drawVectorText(scratch, 0.0f, 0.0f, b.letterSize, b.text, 1.0f, 1.0f, 1.0f, 1.0f);
        const auto& verts2D = scratch.getVertices();

        auto mapPointToSpline3D = [&](float lx, float ly) -> Vec3f {
            float uNorm = (lx / totalW) - 0.5f; // -0.5 .. +0.5
            float t = std::clamp(0.5f + uNorm * tSpan, 0.0f, 1.0f);
            float omt = 1.0f - t;

            // Quadratic Bezier position B(t)
            Vec2f pos2D{
                omt * omt * b.p0.x + 2.0f * omt * t * b.p1.x + t * t * b.p2.x,
                omt * omt * b.p0.y + 2.0f * omt * t * b.p1.y + t * t * b.p2.y
            };
            // Quadratic Bezier tangent B'(t)
            Vec2f tang{
                2.0f * omt * (b.p1.x - b.p0.x) + 2.0f * t * (b.p2.x - b.p1.x),
                2.0f * omt * (b.p1.y - b.p0.y) + 2.0f * t * (b.p2.y - b.p1.y)
            };
            float tLen = std::sqrt(tang.x * tang.x + tang.y * tang.y);
            if (tLen > 1e-5f) { tang.x /= tLen; tang.y /= tLen; }
            else { tang = {1.0f, 0.0f}; }

            // Normal perpendicular to tangent in map XZ plane (pointing "north" / up on map)
            Vec2f norm{-tang.y, tang.x};
            float vOffset = (b.letterSize * 0.5f - ly);

            float wx = pos2D.x + norm.x * vOffset;
            float wz = pos2D.y + norm.y * vOffset;
            float groundY = std::max(0.0f, voronoi_.sampleAt(wx, wz).elevation);
            return {wx, groundY + 0.14f, wz};
        };

        uint32_t packed = packBiomesAndLayer(
            b.isWaterTitle ? BiomeId::OceanDeep : BiomeId::LushMeadow,
            BiomeId::LushMeadow,
            InkLayer::InkCalligraphyInk
        );

        for (size_t i = 0; i + 2 < verts2D.size(); i += 3) {
            for (size_t k = 0; k < 3; ++k) {
                Vec3f p3 = mapPointToSpline3D(verts2D[i + k].x, verts2D[i + k].y);
                ChartVertex cv{};
                cv.pos[0] = p3.x;
                cv.pos[1] = p3.y;
                cv.pos[2] = p3.z;
                cv.sdfCoast = 10.0f;
                cv.normal[0] = 0.0f; cv.normal[1] = 1.0f; cv.normal[2] = 0.0f;
                cv.sdfRoad = 100.0f;
                cv.sdfRiver = 100.0f;
                cv.packedBiomes = packed;
                cv.flags = ChartFlags::IsMapBanner;
                worldVerts_.push_back(cv);
            }
            bannerGlyphTriCount_++;
        }
    };

    for (const auto& b : voronoi_.banners()) {
        emitCurvedBanner(b);
    }
    for (const auto& b : customBanners_) {
        emitCurvedBanner(b);
    }

    // Also place curved local settlement labels right below each POI standee on the parchment!
    for (const auto& p : voronoi_.pois()) {
        CurvedMapBanner poiBanner{};
        poiBanner.text = p.name;
        poiBanner.p0 = {p.pos.x - 5.5f, p.pos.y - 2.4f};
        poiBanner.p1 = {p.pos.x,        p.pos.y - 3.1f};
        poiBanner.p2 = {p.pos.x + 5.5f, p.pos.y - 2.4f};
        poiBanner.letterSize = 1.15f;
        poiBanner.isRegionHeader = false;
        emitCurvedBanner(poiBanner);
    }
}

ChartStats ChartWorld::rebuildWorldGeometry() {
    worldVerts_.clear();
    mapSurfaceTriCount_ = 0;
    bannerGlyphTriCount_ = 0;

    // 1. Sample a continuous 144x144 grid over [-72m, +72m]^2 (1.0m resolution)
    constexpr int kGridN = 144;
    constexpr float kMinCoord = -72.0f;
    constexpr float kStep = 1.0f;

    const int vertCount1D = kGridN + 1;
    std::vector<ChartFieldSample> samples(static_cast<size_t>(vertCount1D) * vertCount1D);
    for (int gz = 0; gz <= kGridN; ++gz) {
        float wz = kMinCoord + static_cast<float>(gz) * kStep;
        for (int gx = 0; gx <= kGridN; ++gx) {
            float wx = kMinCoord + static_cast<float>(gx) * kStep;
            samples[static_cast<size_t>(gz) * vertCount1D + gx] = voronoi_.sampleAt(wx, wz);
        }
    }

    worldVerts_.reserve(static_cast<size_t>(kGridN) * kGridN * 6 + 16384);

    auto makeVertex = [&](int gx, int gz) -> ChartVertex {
        const auto& s = samples[static_cast<size_t>(gz) * vertCount1D + gx];
        float wx = kMinCoord + static_cast<float>(gx) * kStep;
        float wz = kMinCoord + static_cast<float>(gz) * kStep;

        // Compute smooth surface normal from tabletop elevation
        int gxL = std::max(0, gx - 1), gxR = std::min(kGridN, gx + 1);
        int gzD = std::max(0, gz - 1), gzU = std::min(kGridN, gz + 1);
        float hL = samples[static_cast<size_t>(gz)  * vertCount1D + gxL].elevation;
        float hR = samples[static_cast<size_t>(gz)  * vertCount1D + gxR].elevation;
        float hD = samples[static_cast<size_t>(gzD) * vertCount1D + gx ].elevation;
        float hU = samples[static_cast<size_t>(gzU) * vertCount1D + gx ].elevation;

        Vec3f norm{-(hR - hL) * 0.5f, 1.0f, -(hU - hD) * 0.5f};
        float nLen = std::sqrt(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
        if (nLen > 1e-5f) { norm.x /= nLen; norm.y /= nLen; norm.z /= nLen; }

        ChartVertex v{};
        v.pos[0]     = wx;
        v.pos[1]     = s.elevation;
        v.pos[2]     = wz;
        v.sdfCoast   = s.sdfCoast;
        v.normal[0]  = norm.x;
        v.normal[1]  = norm.y;
        v.normal[2]  = norm.z;
        v.sdfRoad    = s.sdfRoad;
        v.gradXZ[0]  = s.gradX;
        v.gradXZ[1]  = s.gradZ;
        v.sdfRiver   = s.sdfRiver - s.riverWidth; // Signed distance to river bank (<0 inside river!)
        v.biomeBlend = s.biomeBlend;
        v.uv[0]      = s.roadArc;
        v.uv[1]      = s.riverWidth;
        v.packedBiomes = packBiomesAndLayer(s.biomePrimary, s.biomeSecondary, InkLayer::ParchmentGrain);
        v.flags      = s.flags;
        return v;
    };

    for (int gz = 0; gz < kGridN; ++gz) {
        for (int gx = 0; gx < kGridN; ++gx) {
            ChartVertex v00 = makeVertex(gx,     gz);
            ChartVertex v10 = makeVertex(gx + 1, gz);
            ChartVertex v11 = makeVertex(gx + 1, gz + 1);
            ChartVertex v01 = makeVertex(gx,     gz + 1);

            worldVerts_.push_back(v00);
            worldVerts_.push_back(v01);
            worldVerts_.push_back(v10);

            worldVerts_.push_back(v10);
            worldVerts_.push_back(v01);
            worldVerts_.push_back(v11);

            mapSurfaceTriCount_ += 2;
        }
    }

    // 2. Bake 3D Curved Calligraphic Map Banners onto the parchment surface
    bakeCurvedMapBannersToMesh();

    // 3. Build initial dynamic standees & combat telegraphs
    buildDynamicStandeesAndTelegraphs();

    ChartStats stats{};
    stats.atlasGenTimeMs       = atlasGenTimeMs_;
    stats.mapSurfaceTriangles  = mapSurfaceTriCount_;
    stats.bannerGlyphTriangles = bannerGlyphTriCount_;
    stats.standeeCount         = static_cast<uint32_t>(activeStandees_.size());
    stats.telegraphCount       = static_cast<uint32_t>(activeTelegraphs_.size());
    stats.pointLightCount      = static_cast<uint32_t>(pointLights_.size());
    return stats;
}

void ChartWorld::moveHero(float moveX, float moveZ, bool sprintOrDodge, float dt) {
    if (hero_.dodgeCooldown > 0.0f) hero_.dodgeCooldown = std::max(0.0f, hero_.dodgeCooldown - dt);
    if (hero_.dodgeTimer > 0.0f)    hero_.dodgeTimer    = std::max(0.0f, hero_.dodgeTimer - dt);

    float len = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (len > 1e-4f) {
        moveX /= len;
        moveZ /= len;
        hero_.facingDir = {moveX, moveZ};
        if (std::abs(moveX) > 0.08f) {
            hero_.facingRight = (moveX > 0.0f);
        }

        if (sprintOrDodge && hero_.dodgeCooldown <= 0.0f) {
            hero_.dodgeTimer = 0.24f;
            hero_.dodgeCooldown = 0.55f;
        }
    }

    float speed = hero_.isSailing ? 14.5f : 10.5f;
    if (hero_.dodgeTimer > 0.0f) {
        speed *= 2.35f;
    } else if (sprintOrDodge) {
        speed *= 1.55f;
    }

    float nextX = std::clamp(hero_.pos.x + moveX * speed * dt, -68.0f, 68.0f);
    float nextZ = std::clamp(hero_.pos.z + moveZ * speed * dt, -68.0f, 68.0f);
    ChartFieldSample s = voronoi_.sampleAt(nextX, nextZ);

    if (voronoi_.isDungeonMode()) {
        // Collide against hatched One-Page Dungeon walls
        if (s.biomePrimary != BiomeId::DungeonHatchedWall) {
            hero_.pos.x = nextX;
            hero_.pos.z = nextZ;
            hero_.pos.y = std::max(0.0f, s.elevation);
        }
    } else {
        hero_.pos.x = nextX;
        hero_.pos.z = nextZ;
        hero_.pos.y = std::max(0.0f, s.elevation);
        // Seamless Cat Quest Land <-> Naval Ship transition at the shoreline!
        hero_.isSailing = (s.sdfCoast < -0.45f && (s.flags & ChartFlags::IsBridge) == 0u);
    }

    // Update active POI region banner when approaching landmarks
    float bestPoiDist = 18.0f;
    for (const auto& p : voronoi_.pois()) {
        float dx = hero_.pos.x - p.pos.x;
        float dz = hero_.pos.z - p.pos.y;
        float d = std::sqrt(dx * dx + dz * dz);
        if (d < bestPoiDist) {
            bestPoiDist = d;
            hero_.activeRegionName = p.name;
            hero_.activeRegionSub  = p.subtitle;
        }
    }
}

void ChartWorld::triggerMeleeSlash() {
    if (hero_.attackTimer > 0.05f) return;
    hero_.attackTimer = 0.28f;

    // Hit enemies within sword arc
    Vec2f slashCenter{
        hero_.pos.x + hero_.facingDir.x * 2.2f,
        hero_.pos.z + hero_.facingDir.y * 2.2f
    };

    for (auto& en : enemies_) {
        if (!en.alive) continue;
        float dx = en.pos.x - slashCenter.x;
        float dz = en.pos.z - slashCenter.y;
        float d  = std::sqrt(dx * dx + dz * dz);
        if (d < en.width * 0.85f + 2.4f) {
            int dmg = 18 + (hero_.level * 4);
            en.hp -= dmg;
            en.hitFlashTimer = 0.22f;
            // Knockback
            if (d > 1e-4f) {
                en.pos.x += (dx / d) * 1.1f;
                en.pos.z += (dz / d) * 1.1f;
            }
            floatingNumbers_.push_back(FloatingNumber{
                {en.pos.x, en.pos.y + en.height, en.pos.z},
                std::to_string(dmg),
                RGBA8{255, 235, 95, 255},
                0.85f, 0.0f
            });

            if (en.hp <= 0) {
                en.alive = false;
                hero_.xp += en.isBoss ? 120 : 35;
                hero_.gold += en.isBoss ? 100 : 20;
                if (hero_.xp >= hero_.level * 100) {
                    hero_.xp -= hero_.level * 100;
                    hero_.level++;
                    hero_.maxHp += 25;
                    hero_.hp = hero_.maxHp;
                    floatingNumbers_.push_back(FloatingNumber{
                        {hero_.pos.x, hero_.pos.y + 3.0f, hero_.pos.z},
                        "LEVEL UP!",
                        RGBA8{110, 245, 165, 255},
                        1.4f, 0.0f
                    });
                }
            }
        }
    }
}

void ChartWorld::triggerArcaneSpell() {
    if (hero_.spellTimer > 0.1f) return;
    hero_.spellTimer = 0.65f;

    for (auto& en : enemies_) {
        if (!en.alive) continue;
        float dx = en.pos.x - hero_.pos.x;
        float dz = en.pos.z - hero_.pos.z;
        float d  = std::sqrt(dx * dx + dz * dz);
        if (d <= 5.8f) {
            int dmg = 32 + (hero_.level * 6);
            en.hp -= dmg;
            en.hitFlashTimer = 0.28f;
            floatingNumbers_.push_back(FloatingNumber{
                {en.pos.x, en.pos.y + en.height + 0.4f, en.pos.z},
                std::to_string(dmg) + "!",
                RGBA8{115, 215, 255, 255},
                0.95f, 0.0f
            });
            if (en.hp <= 0) {
                en.alive = false;
                hero_.xp += 40;
                hero_.gold += 25;
            }
        }
    }
}

bool ChartWorld::tryToggleNearbyDungeon() {
    if (voronoi_.isDungeonMode()) {
        // Return to the Overworld chart at our saved position!
        Vec3f retPos = overworldReturnPos_;
        generateCustomChart(overworldReturnSeed_, overworldReturnPreset_);
        hero_.pos = retPos;
        rebuildWorldGeometry();
        return true;
    }

    for (const auto& p : voronoi_.pois()) {
        if (p.category == PoiCategory::DungeonPortal || p.category == PoiCategory::SunkenRuin) {
            float dx = hero_.pos.x - p.pos.x;
            float dz = hero_.pos.z - p.pos.y;
            if (std::sqrt(dx * dx + dz * dz) < 6.5f) {
                overworldReturnPos_ = hero_.pos;
                buildOnePageDungeon(p.seed);
                rebuildWorldGeometry();
                return true;
            }
        }
    }
    return false;
}

void ChartWorld::buildDynamicStandeesAndTelegraphs() {
    activeStandees_ = staticStandees_;
    activeTelegraphs_.clear();

    // 1. Quest / Portal Beacon ring on the parchment around nearby Dungeon Portals
    for (const auto& p : voronoi_.pois()) {
        if (p.category == PoiCategory::DungeonPortal || p.category == PoiCategory::BossLair) {
            CombatTelegraph q{};
            q.center[0] = p.pos.x;
            q.center[1] = p.elevation;
            q.center[2] = p.pos.y;
            q.radius    = 2.8f;
            q.progress  = 0.5f + 0.5f * std::sin(simTimeSec_ * 2.5f);
            q.telegraphType = TelegraphType::QuestBeacon;
            activeTelegraphs_.push_back(q);
        }
    }

    // 2. Player Arcane Spell Telegraph Ring (Cat Quest Astropaw / Flamepurr nova)
    if (hero_.spellTimer > 0.0f) {
        CombatTelegraph sp{};
        sp.center[0] = hero_.pos.x;
        sp.center[1] = hero_.pos.y;
        sp.center[2] = hero_.pos.z;
        sp.radius    = 5.6f;
        sp.progress  = 1.0f - (hero_.spellTimer / 0.65f);
        sp.telegraphType = TelegraphType::PlayerArcane;
        activeTelegraphs_.push_back(sp);
    }

    // 3. Hero Standee (Armored Cat Adventurer on land, Inked Caravel Ship at sea!)
    StandeeInstance heroSt{};
    heroSt.worldPos[0] = hero_.pos.x;
    heroSt.worldPos[1] = hero_.pos.y;
    heroSt.worldPos[2] = hero_.pos.z;
    heroSt.width       = hero_.isSailing ? 2.8f : 2.2f;
    heroSt.height      = hero_.isSailing ? 2.9f : 2.4f;
    heroSt.texLayer    = hero_.isSailing ? InkLayer::StandeeShipCaravel : InkLayer::StandeeHeroCat;
    heroSt.flags       = StandeeFlags::IdleBob | StandeeFlags::PopUpTilt | (hero_.facingRight ? 0u : StandeeFlags::FlipX);
    heroSt.phaseOffset = simTimeSec_ * 3.0f;
    activeStandees_.push_back(heroSt);

    // 4. Floating Companion Spirit Standee
    StandeeInstance compSt{};
    compSt.worldPos[0] = hero_.pos.x - (hero_.facingRight ? 1.5f : -1.5f);
    compSt.worldPos[1] = hero_.pos.y + 0.8f + std::sin(simTimeSec_ * 3.2f) * 0.22f;
    compSt.worldPos[2] = hero_.pos.z + 0.6f;
    compSt.width       = 1.3f;
    compSt.height      = 1.3f;
    compSt.texLayer    = InkLayer::StandeeHeroCompanion;
    compSt.flags       = StandeeFlags::Emissive | StandeeFlags::IdleBob;
    compSt.phaseOffset = simTimeSec_;
    activeStandees_.push_back(compSt);

    // 5. Active Melee Slash Crescent
    if (hero_.attackTimer > 0.0f) {
        StandeeInstance slashSt{};
        slashSt.worldPos[0] = hero_.pos.x + hero_.facingDir.x * 1.9f;
        slashSt.worldPos[1] = hero_.pos.y + 0.2f;
        slashSt.worldPos[2] = hero_.pos.z + hero_.facingDir.y * 1.9f;
        slashSt.width       = 2.6f;
        slashSt.height      = 2.4f;
        slashSt.texLayer    = InkLayer::StandeeSlashArc;
        slashSt.flags       = StandeeFlags::Emissive | (hero_.facingRight ? 0u : StandeeFlags::FlipX);
        activeStandees_.push_back(slashSt);
    }

    // 6. Live Enemies & their Ground-Projected Combat Telegraphs
    for (const auto& en : enemies_) {
        if (!en.alive) continue;
        StandeeInstance es{};
        es.worldPos[0] = en.pos.x;
        es.worldPos[1] = en.pos.y;
        es.worldPos[2] = en.pos.z;
        es.width       = en.width;
        es.height      = en.height;
        es.texLayer    = en.standeeLayer;
        es.flags       = StandeeFlags::IdleBob | StandeeFlags::PopUpTilt |
                         ((en.hitFlashTimer > 0.0f) ? StandeeFlags::HitFlash : 0u);
        es.phaseOffset = en.pos.x + en.pos.z;
        activeStandees_.push_back(es);

        if (en.telegraphProgress > 0.01f) {
            CombatTelegraph ct{};
            ct.center[0] = en.pos.x;
            ct.center[1] = en.pos.y;
            ct.center[2] = en.pos.z;
            ct.radius    = en.telegraphRadius;
            float dx = hero_.pos.x - en.pos.x;
            float dz = hero_.pos.z - en.pos.z;
            float dl = std::sqrt(dx * dx + dz * dz);
            ct.dirX      = (dl > 1e-4f) ? (dx / dl) : 0.0f;
            ct.dirZ      = (dl > 1e-4f) ? (dz / dl) : -1.0f;
            ct.progress  = en.telegraphProgress;
            ct.telegraphType = en.telegraphType;
            activeTelegraphs_.push_back(ct);
        }
    }
}

void ChartWorld::updateSimulation(float dt, uint32_t viewportW, uint32_t viewportH) {
    simTimeSec_ += dt;
    if (hero_.attackTimer > 0.0f) hero_.attackTimer = std::max(0.0f, hero_.attackTimer - dt);
    if (hero_.spellTimer > 0.0f)  hero_.spellTimer  = std::max(0.0f, hero_.spellTimer - dt);

    // Update enemy AI & Cat Quest style ground telegraph attacks
    for (auto& en : enemies_) {
        if (!en.alive) continue;
        if (en.hitFlashTimer > 0.0f) en.hitFlashTimer = std::max(0.0f, en.hitFlashTimer - dt);

        float dx = hero_.pos.x - en.pos.x;
        float dz = hero_.pos.z - en.pos.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (en.telegraphProgress > 0.0f) {
            en.telegraphProgress += dt * (en.isBoss ? 0.55f : 0.72f);
            if (en.telegraphProgress >= 1.0f) {
                // Telegraph completes -> trigger enemy strike if hero didn't dodge out of the circle!
                if (dist <= en.telegraphRadius && hero_.dodgeTimer <= 0.0f) {
                    int dmg = en.isBoss ? 18 : 9;
                    hero_.hp = std::max(1, hero_.hp - dmg);
                    floatingNumbers_.push_back(FloatingNumber{
                        {hero_.pos.x, hero_.pos.y + 2.4f, hero_.pos.z},
                        "-" + std::to_string(dmg),
                        RGBA8{255, 92, 82, 255},
                        0.85f, 0.0f
                    });
                }
                en.telegraphProgress = 0.0f;
                en.cooldownTimer = en.isBoss ? 1.6f : 2.2f;
            }
        } else {
            if (en.cooldownTimer > 0.0f) {
                en.cooldownTimer = std::max(0.0f, en.cooldownTimer - dt);
            }
            if (dist < 14.0f && dist > en.telegraphRadius * 0.7f) {
                // Approach hero on the map plane
                float step = (en.isBoss ? 3.2f : 2.6f) * dt;
                en.pos.x += (dx / dist) * step;
                en.pos.z += (dz / dist) * step;
                en.pos.y = std::max(0.0f, voronoi_.sampleAt(en.pos.x, en.pos.z).elevation);
            } else if (dist <= en.telegraphRadius * 0.95f && en.cooldownTimer <= 0.0f) {
                // Begin charging ground-projected attack telegraph!
                en.telegraphProgress = 0.02f;
            }
        }
    }

    // Update floating damage numbers
    for (auto& fn : floatingNumbers_) {
        fn.age += dt;
        fn.worldPos.y += dt * 1.8f;
    }
    floatingNumbers_.erase(
        std::remove_if(floatingNumbers_.begin(), floatingNumbers_.end(),
                       [](const FloatingNumber& f) { return f.age >= f.lifetime; }),
        floatingNumbers_.end()
    );

    buildDynamicStandeesAndTelegraphs();
    buildHudAndMapOverlay(viewportW, viewportH);
}

void ChartWorld::buildHudAndMapOverlay(uint32_t viewportW, uint32_t viewportH) {
    float vpW = static_cast<float>(std::max(1u, viewportW));
    float vpH = static_cast<float>(std::max(1u, viewportH));

    overlay2D_.beginFrame(vpW, vpH);

    // Top Cartographic Parchment Ribbon Banner
    float barW = std::min(780.0f, vpW - 32.0f);
    float barX = (vpW - barW) * 0.5f;
    float barY = 12.0f;
    float barH = 44.0f;

    overlay2D_.drawRoundedRect(barX, barY, barW, barH, 6.0f, 0.14f, 0.11f, 0.08f, 0.86f);
    overlay2D_.drawRoundedRectOutline(barX, barY, barW, barH, 6.0f, 0.82f, 0.68f, 0.42f, 0.92f, 1.5f);

    // Left: Active Region / POI Title
    std::string regionHeader = hero_.activeRegionName + (hero_.isSailing ? " [SAILING]" : "");
    eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + 16.0f, barY + 8.0f, 15.0f, regionHeader, 0.96f, 0.88f, 0.68f, 1.0f);
    eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + 16.0f, barY + 26.0f, 11.0f, hero_.activeRegionSub, 0.78f, 0.72f, 0.58f, 0.95f);

    // Right: Cat Quest Hero Stats (Lv, HP, Gold, Controls)
    char statBuf[128];
    std::snprintf(statBuf, sizeof(statBuf), "LV.%d  HP %d/%d  GOLD %d", hero_.level, hero_.hp, hero_.maxHp, hero_.gold);
    float statW = eatsbits::ui::glyph::getVectorTextWidth(13.0f, statBuf);
    eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + barW - statW - 16.0f, barY + 9.0f, 13.0f, statBuf, 0.98f, 0.82f, 0.38f, 1.0f);

    const char* ctrlHint = "WASD:Move  SPACE/LClick:Slash  K/RClick:Spell  E:Dungeon  M:Atlas  1-4:Charts";
    float hintW = eatsbits::ui::glyph::getVectorTextWidth(10.0f, ctrlHint);
    eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + barW - hintW - 16.0f, barY + 27.0f, 10.0f, ctrlHint, 0.76f, 0.74f, 0.68f, 0.9f);
}

RaycastHit ChartWorld::raycast(Vec3f origin, Vec3f dir, float maxDist) const noexcept {
    RaycastHit result{};
    float step = 0.75f;
    for (float t = 1.0f; t <= maxDist; t += step) {
        Vec3f p{origin.x + dir.x * t, origin.y + dir.y * t, origin.z + dir.z * t};
        if (std::abs(p.x) <= 72.0f && std::abs(p.z) <= 72.0f) {
            ChartFieldSample s = voronoi_.sampleAt(p.x, p.z);
            if (p.y <= s.elevation) {
                result.hit = true;
                result.distance = t;
                result.position = {p.x, s.elevation, p.z};
                result.biome = s.biomePrimary;
                return result;
            }
        }
        step = std::min(2.5f, step * 1.015f);
    }
    return result;
}

} // namespace eatscharted
