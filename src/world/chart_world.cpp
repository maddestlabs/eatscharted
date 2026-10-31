#include "eatscharted/chart_world.hpp"
#include "eatscharted/cartographic_text.hpp"
#include "eatscharted/audio_foley.hpp"
#include "eatsbits/ui/glyph/vector_font.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace eatscharted {

namespace {

[[nodiscard]] uint32_t hashU32(uint32_t x) noexcept {
    x ^= x >> 16u;
    x *= 0x7FEB352Du;
    x ^= x >> 15u;
    x *= 0x846CA68Bu;
    x ^= x >> 16u;
    return x;
}

[[nodiscard]] uint32_t hashPos(int x, int z, uint32_t seed) noexcept {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(z) * 668265263u +
                 seed * 1442695041u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}



[[nodiscard]] uint64_t packChunkKey(int cx, int cz) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32u) |
            static_cast<uint64_t>(static_cast<uint32_t>(cz));
}

} // namespace

ChartWorld::ChartWorld() {
    auto t0 = std::chrono::high_resolution_clock::now();
    inkAtlas_ = InkCanvas::generateFullAtlas(1492u);
    auto t1 = std::chrono::high_resolution_clock::now();
    atlasGenTimeMs_ = std::chrono::duration<float, std::milli>(t1 - t0).count();
    atlasGenerated_ = true;
    overlay2D_.setAntiAliasingMode(2);
}

void ChartWorld::clear() {
    dungeonExplicitDressing_ = false;
    chunkCache_.clear();
    activeChunkX_ = -999999;
    activeChunkZ_ = -999999;
    worldVerts_.clear();
    staticStandees_.clear();
    customStandees_.clear();
    activeStandees_.clear();
    activeTelegraphs_.clear();
    pointLights_.clear();
    staticChunkLights_.clear();
    customPointLights_.clear();
    customBanners_.clear();
    worldBanners_.clear();
    enemies_.clear();
    floatingNumbers_.clear();
    localeBanner_.active = false;
    lastDiscoveredLocale_.clear();
    mapSurfaceTriCount_ = 0;
    bannerGlyphTriCount_ = 0;
}

void ChartWorld::triggerLocaleDiscovery(std::string_view name, std::string_view subtitle) {
    if (name.empty()) return;
    localeBanner_.title = std::string(name);
    localeBanner_.subtitle = std::string(subtitle);
    localeBanner_.timer = 0.0f;
    localeBanner_.duration = 2.8f;
    localeBanner_.active = true;
    lastDiscoveredLocale_ = localeBanner_.title;
}

void ChartWorld::buildPerilousPeninsula(uint32_t seed) {
    overworldReturnSeed_ = seed;
    overworldReturnPreset_ = AtlasPreset::PerilousPeninsula;
    generateCustomChart(seed, AtlasPreset::PerilousPeninsula);
    hero_.pos = {6.0f, 0.45f, -30.0f};
    hero_.activeRegionName = "LOWMILLBURG";
    hero_.activeRegionSub  = "Kildland Realm";
    triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);
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
    triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);
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
    triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 28.0f, hero_.pos.z - 25.0f};
}

void ChartWorld::buildSunscorchedWastes(uint32_t seed) {
    overworldReturnSeed_ = seed;
    overworldReturnPreset_ = AtlasPreset::SunscorchedWastes;
    generateCustomChart(seed, AtlasPreset::SunscorchedWastes);
    hero_.pos = {12.0f, 0.45f, -6.0f};
    hero_.isSailing = false;
    hero_.activeRegionName = "AL-ZAHRA OASIS";
    hero_.activeRegionSub  = "The Sunscorched Expanse";
    triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 28.0f, hero_.pos.z - 25.0f};
}

void ChartWorld::buildFrostfellPeaks(uint32_t seed) {
    overworldReturnSeed_ = seed;
    overworldReturnPreset_ = AtlasPreset::FrostfellPeaks;
    generateCustomChart(seed, AtlasPreset::FrostfellPeaks);
    hero_.pos = {-24.0f, 1.25f, -14.0f};
    hero_.isSailing = false;
    hero_.activeRegionName = "RIMEVALE HAMLET";
    hero_.activeRegionSub  = "The Frostfell Reach";
    triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 28.0f, hero_.pos.z - 25.0f};

    // Initialize atmospheric frosty polar mood
    atmosphere_.skyZenith = {0.18f, 0.28f, 0.44f};
    atmosphere_.skyHorizon = {0.72f, 0.78f, 0.88f};
    atmosphere_.sunColor = {0.92f, 0.95f, 1.0f};
    atmosphere_.fogDensity = 0.0035f;

    // Start with delicate highland snow flurries and polar wind
    weather_.type = WeatherType::Flurries;
    weather_.intensity = 0.35f;
    weather_.snowCover = 0.70f;
    weather_.autoCycle = true;
}

void ChartWorld::buildContinentalPangaea(uint32_t seed) {
    overworldReturnSeed_ = seed;
    overworldReturnPreset_ = AtlasPreset::ContinentalPangaea;
    generateCustomChart(seed, AtlasPreset::ContinentalPangaea);
    hero_.pos = {0.0f, 1.75f, 0.0f};
    hero_.isSailing = false;
    hero_.activeRegionName = "THE IMPERIAL CROSSROADS";
    hero_.activeRegionSub  = "Heart of Pangaea";
    triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);
    camera_.target   = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 28.0f, hero_.pos.z - 25.0f};
}

void ChartWorld::initDungeon(uint32_t seed, std::string_view title, std::string_view subtitle) {
    clear();
    gridCenter_ = {0.0f, 0.0f};
    voronoi_.setPreset(AtlasPreset::OnePageDungeon);
    voronoi_.setSeed(seed);
    voronoi_.clearDungeon();
    dungeonExplicitDressing_ = true;

    // Subterranean dark torchlit mood: Zero celestial sun by default in dungeons
    atmosphere_.skyZenith = {0.02f, 0.02f, 0.03f};
    atmosphere_.skyHorizon = {0.03f, 0.03f, 0.04f};
    atmosphere_.sunColor = {0.0f, 0.0f, 0.0f}; // Zero celestial sun by default inside subterranean dungeons
    atmosphere_.sunDirection = {-0.2f, 0.9f, -0.3f};
    atmosphere_.fogDensity = 0.003f;
    atmosphere_.exposure = 1.05f;
    useDayNightCycle_ = false; // Strictly disabled by default in dungeons

    hero_.pos = {0.0f, 0.28f, -28.0f};
    hero_.isSailing = false;
    hero_.activeRegionName = std::string(title);
    hero_.activeRegionSub = std::string(subtitle);
    triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);

    camera_.target = {hero_.pos.x, hero_.pos.y, hero_.pos.z};
    camera_.position = {hero_.pos.x, 26.0f, hero_.pos.z - 22.0f};
    camera_.fovDegrees = 44.0f;
    camera_.horizonCurve = 0.0f;
}

void ChartWorld::addDungeonRoom(float minX, float minZ, float maxX, float maxZ, std::string_view label, bool water, bool boss) {
    int id = static_cast<int>(voronoi_.dungeonRooms().size());
    voronoi_.addDungeonRoom(id, minX, minZ, maxX, maxZ, label, water, boss);
    // Note: No floor text is rendered in dungeons; the animated header toast is sufficient.
}

void ChartWorld::addDungeonCorridor(float x1, float z1, float x2, float z2, float width) {
    voronoi_.addDungeonCorridor(x1, z1, x2, z2, width);
}

void ChartWorld::addDungeonWaterPool(float minX, float minZ, float maxX, float maxZ) {
    voronoi_.addDungeonWaterPool(minX, minZ, maxX, maxZ);
}

void ChartWorld::addDungeonTorch(Vec3f pos, Vec3f color, float radius, float intensity) {
    addCustomStandee(pos, 1.4f, 1.5f, InkLayer::StandeeCampfire, StandeeFlags::Emissive | StandeeFlags::IdleBob);
    addPointLight({pos.x, pos.y + 1.8f, pos.z}, radius, color, intensity);
}

void ChartWorld::addDungeonPillar(Vec3f pos) {
    addCustomStandee(pos, 1.6f, 3.2f, InkLayer::StandeeStandingStone, StandeeFlags::PopUpTilt);
}

void ChartWorld::addDungeonChest(Vec3f pos) {
    addCustomStandee(pos, 1.8f, 1.6f, InkLayer::StandeeTreasureChest, StandeeFlags::PopUpTilt);
}

void ChartWorld::addDungeonExitPortal(Vec3f pos) {
    addCustomStandee(pos, 3.6f, 3.8f, InkLayer::StandeeDungeonArch, StandeeFlags::PopUpTilt | StandeeFlags::Emissive);
    addPointLight({pos.x, pos.y + 2.0f, pos.z}, 14.0f, {0.6f, 0.85f, 1.0f}, 1.4f);
}

void ChartWorld::addDungeonEnemy(Vec3f pos, std::string_view name, uint16_t layer, int hp, bool isBoss, float telegraphRadius, uint32_t telegraphType) {
    EnemyAgent en{};
    en.pos = pos;
    en.homePos = pos;
    en.standeeLayer = layer;
    en.name = std::string(name);
    en.width = isBoss ? 4.6f : 2.2f;
    en.height = isBoss ? 4.6f : 2.2f;
    en.hp = hp;
    en.maxHp = hp;
    en.telegraphRadius = telegraphRadius;
    en.telegraphType = telegraphType;
    en.isBoss = isBoss;
    en.alive = true;
    enemies_.push_back(en);
}

void ChartWorld::buildWatabouDungeon(uint32_t seed, DungeonTheme theme) {
    if (theme == DungeonTheme::Auto) {
        uint32_t thChoice = ((seed >> 4) ^ (seed >> 12)) % 4;
        theme = static_cast<DungeonTheme>(thChoice + 1);
    }
    std::string_view title = "THE SUNKEN SANCTUM";
    std::string_view sub = "Flooded Subterranean Cistern";
    if (theme == DungeonTheme::WhisperingCrypt) {
        title = "THE WHISPERING CRYPT";
        sub = "Ancient Ossuary & Tombs";
    } else if (theme == DungeonTheme::MoltenVault) {
        title = "THE MOLTEN CRUCIBLE";
        sub = "Subterranean Magma Vault";
    } else if (theme == DungeonTheme::GildedReliquary) {
        title = "THE GILDED RELIQUARY";
        sub = "Alchemical Treasury & Font";
    }

    // 1. Initialize dungeon mode (sets preset, baseline atmosphere, clears old world)
    initDungeon(seed, title, sub);

    // 2. Generate the procedural Watabou layout (rooms, corridors, pools)
    voronoi_.generateWatabouProcedural(seed, theme);

    // 3. Theme-specific subterranean atmospheric lighting (dark by default, requires torches/lanterns)
    atmosphere_.sunColor = {0.0f, 0.0f, 0.0f}; // Subterranean dungeons have zero direct celestial sunlight by default
    useDayNightCycle_ = false;                  // No day/night cycle underground unless explicitly scripted

    if (theme == DungeonTheme::MoltenVault) {
        atmosphere_.skyZenith = {0.02f, 0.008f, 0.008f};
        atmosphere_.skyHorizon = {0.035f, 0.012f, 0.01f};
    } else if (theme == DungeonTheme::WhisperingCrypt) {
        atmosphere_.skyZenith = {0.01f, 0.01f, 0.015f};
        atmosphere_.skyHorizon = {0.02f, 0.02f, 0.025f};
    } else if (theme == DungeonTheme::GildedReliquary) {
        atmosphere_.skyZenith = {0.015f, 0.012f, 0.01f};
        atmosphere_.skyHorizon = {0.025f, 0.02f, 0.015f};
    } else { // SunkenSanctum / default
        atmosphere_.skyZenith = {0.01f, 0.012f, 0.018f};
        atmosphere_.skyHorizon = {0.02f, 0.022f, 0.028f};
    }

    // 4. Set hero pos in entrance room center
    if (!voronoi_.dungeonRooms().empty()) {
        const auto& r0 = voronoi_.dungeonRooms()[0];
        hero_.pos = {(r0.minX + r0.maxX) * 0.5f, 0.28f, (r0.minZ + r0.maxZ) * 0.5f};
        camera_.target = hero_.pos;
        camera_.position = {hero_.pos.x, 26.0f, hero_.pos.z - 22.0f};
    }

    dungeonExplicitDressing_ = false; // allows bakeChunk to auto-dress rooms, torches, and enemies
    rebuildWorldBanners();
}

void ChartWorld::buildProceduralDungeon(uint32_t seed, DungeonTheme theme) {
    buildWatabouDungeon(seed, theme);
}

void ChartWorld::buildOnePageDungeon(uint32_t seed, DungeonTheme theme) {
    buildWatabouDungeon(seed, theme);
}

void ChartWorld::generateCustomChart(uint32_t seed, AtlasPreset preset) {
    clear();
    gridCenter_ = {0.0f, 0.0f};
    useDayNightCycle_ = true;
    voronoi_.generate(seed, preset);
    rebuildWorldBanners();
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
    customStandees_.push_back(st);
    staticStandees_.push_back(st);
    activeStandees_ = staticStandees_;
    meshDirty_ = true;
}

void ChartWorld::rebuildWorldBanners() {
    worldBanners_.clear();
    worldBanners_.reserve(voronoi_.banners().size() + voronoi_.pois().size() + customBanners_.size());

    // 1. Macro Regional & Oceanic Banners from Voronoi Atlas (authored grand curves across provinces)
    for (const auto& b : voronoi_.banners()) {
        if (!b.text.empty()) {
            worldBanners_.push_back(b);
        }
    }

    // 2. Landmark / Settlement POI Banners (fitted to terrain & de-conflicted against regional banners and each other)
    for (const auto& poi : voronoi_.pois()) {
        if (poi.name.empty()) continue;
        bool isWaterPoi = (poi.category == PoiCategory::SunkenRuin || poi.category == PoiCategory::PortHarbor);
        auto fit = CartographicPathText::fitBanner(
            poi.name,
            poi.pos,
            2.2f, // Cat Quest hero scale (~2.2m)
            voronoi_,
            worldBanners_,
            isWaterPoi,
            /*isRegionHeader=*/false
        );
        worldBanners_.push_back(fit.banner);
    }

    // 3. Custom / Scripted Banners
    for (const auto& b : customBanners_) {
        if (!b.text.empty()) {
            worldBanners_.push_back(b);
        }
    }
}

void ChartWorld::addCustomBanner(const std::string& text, Vec2f p0, Vec2f p1, Vec2f p2, float letterSize) {
    customBanners_.push_back(CurvedMapBanner{text, p0, p1, p2, letterSize, true, false});
    rebuildWorldBanners();
    meshDirty_ = true;
}

void ChartWorld::addPointLight(Vec3f pos, float radius, Vec3f color, float intensity) {
    if (customPointLights_.size() >= 32) return;
    PointLight pl{};
    pl.position[0] = pos.x;
    pl.position[1] = pos.y;
    pl.position[2] = pos.z;
    pl.radius      = radius;
    pl.color[0]    = color.x;
    pl.color[1]    = color.y;
    pl.color[2]    = color.z;
    pl.intensity   = intensity;
    customPointLights_.push_back(pl);
}

void ChartWorld::emitBannerToChunk(const CurvedMapBanner& b, std::vector<ChartVertex>& outVerts,
                                   float minX, float maxX, float minZ, float maxZ) {
    if (b.text.empty()) return;

    auto path = CartographicPathText::generateBezierPath(b.p0, b.p1, b.p2, 64);
    CartographicPathText::emitTextAlongPath(
        b.text,
        path,
        b.letterSize,
        voronoi_,
        outVerts,
        minX, maxX, minZ, maxZ,
        b.isWaterTitle
    );
}

MapChunk ChartWorld::bakeChunk(int cx, int cz) {
    MapChunk ch{};
    ch.cx = cx;
    ch.cz = cz;
    int heroCx = static_cast<int>(std::floor(hero_.pos.x / kChunkSize));
    int heroCz = static_cast<int>(std::floor(hero_.pos.z / kChunkSize));
    if (std::abs(cx - heroCx) <= 1 && std::abs(cz - heroCz) <= 1) {
        ch.discovery = 1.0f;
    } else {
        ch.discovery = 0.0f;
    }

    const float minX = static_cast<float>(cx) * kChunkSize;
    const float minZ = static_cast<float>(cz) * kChunkSize;
    const float maxX = minX + kChunkSize;
    const float maxZ = minZ + kChunkSize;

    voronoi_.ensureRegionBlocks(minX + 16.0f, minZ + 16.0f, 128.0f);

    // 1. Terrain Mesh Generation (32x32 Quads with 1-ring halo for continuous C1 normals)
    constexpr int kGridN = 32;
    constexpr int kHaloDim = kGridN + 3; // 35 (-1 to 33)
    const bool dungeonMode = voronoi_.isDungeonMode();
    std::vector<float> haloH;
    std::vector<float> haloM;

    if (!dungeonMode) {
        haloH.resize(kHaloDim * kHaloDim);
        haloM.resize(kHaloDim * kHaloDim);
        for (int lz = 0; lz < kHaloDim; ++lz) {
            float wz = minZ + static_cast<float>(lz - 1) * 1.0f;
            for (int lx = 0; lx < kHaloDim; ++lx) {
                float wx = minX + static_cast<float>(lx - 1) * 1.0f;
                size_t idx = static_cast<size_t>(lz) * kHaloDim + lx;
                haloH[idx] = voronoi_.rawElevation(wx, wz);
                haloM[idx] = voronoi_.rawMoisture(wx, wz);
            }
        }
    }

    std::vector<ChartFieldSample> samples(kHaloDim * kHaloDim);

    for (int lz = 0; lz < kHaloDim; ++lz) {
        float wz = minZ + static_cast<float>(lz - 1) * 1.0f;
        for (int lx = 0; lx < kHaloDim; ++lx) {
            float wx = minX + static_cast<float>(lx - 1) * 1.0f;
            size_t hIdx = static_cast<size_t>(lz) * kHaloDim + lx;
            if (dungeonMode) {
                samples[hIdx] = voronoi_.sampleAt(wx, wz);
            } else {
                float hC = haloH[hIdx];
                float mC = haloM[hIdx];

                int lxL = std::max(0, lx - 1);
                int lxR = std::min(kHaloDim - 1, lx + 1);
                int lzD = std::max(0, lz - 1);
                int lzU = std::min(kHaloDim - 1, lz + 1);

                float gradX = (haloH[static_cast<size_t>(lz) * kHaloDim + lxR] -
                               haloH[static_cast<size_t>(lz) * kHaloDim + lxL]) * 0.5f;
                float gradZ = (haloH[static_cast<size_t>(lzU) * kHaloDim + lx] -
                               haloH[static_cast<size_t>(lzD) * kHaloDim + lx]) * 0.5f;

                samples[hIdx] = voronoi_.sampleAtKnown(wx, wz, hC, mC, gradX, gradZ);
            }
        }
    }

    auto makeVertex = [&](int gx, int gz) -> ChartVertex {
        int lx = gx + 1;
        int lz = gz + 1;
        const auto& s = samples[static_cast<size_t>(lz) * kHaloDim + lx];
        float wx = minX + static_cast<float>(gx) * 1.0f;
        float wz = minZ + static_cast<float>(gz) * 1.0f;

        // Continuous C1 analytical surface normal across all chunk borders via halo central differences
        float eL = samples[static_cast<size_t>(lz) * kHaloDim + (lx - 1)].elevation;
        float eR = samples[static_cast<size_t>(lz) * kHaloDim + (lx + 1)].elevation;
        float eD = samples[static_cast<size_t>(lz - 1) * kHaloDim + lx].elevation;
        float eU = samples[static_cast<size_t>(lz + 1) * kHaloDim + lx].elevation;
        float dHdX = (eR - eL) * 0.5f;
        float dHdZ = (eU - eD) * 0.5f;

        Vec3f norm{-dHdX, 1.0f, -dHdZ};
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
        v.gradXZ[0]  = dHdX;
        v.gradXZ[1]  = dHdZ;
        v.sdfRiver   = s.sdfRiver - s.riverWidth;
        v.biomeBlend = s.biomeBlend;
        v.uv[0]      = s.roadArc;
        v.uv[1]      = s.riverWidth;
        v.packedBiomes = packBiomesAndLayer(s.biomePrimary, s.biomeSecondary, InkLayer::ParchmentGrain);
        v.flags      = s.flags;
        return v;
    };

    // Precompute the 33x33 unique grid vertices once instead of recomputing 4x per quad
    constexpr int kVertDim = kGridN + 1;
    std::vector<ChartVertex> gridVerts(kVertDim * kVertDim);
    for (int gz = 0; gz < kVertDim; ++gz) {
        for (int gx = 0; gx < kVertDim; ++gx) {
            gridVerts[static_cast<size_t>(gz) * kVertDim + gx] = makeVertex(gx, gz);
        }
    }

    ch.terrainVerts.reserve(kGridN * kGridN * 6);
    for (int gz = 0; gz < kGridN; ++gz) {
        for (int gx = 0; gx < kGridN; ++gx) {
            const ChartVertex& v00 = gridVerts[static_cast<size_t>(gz) * kVertDim + gx];
            const ChartVertex& v10 = gridVerts[static_cast<size_t>(gz) * kVertDim + (gx + 1)];
            const ChartVertex& v11 = gridVerts[static_cast<size_t>(gz + 1) * kVertDim + (gx + 1)];
            const ChartVertex& v01 = gridVerts[static_cast<size_t>(gz + 1) * kVertDim + gx];

            ch.terrainVerts.push_back(v00);
            ch.terrainVerts.push_back(v01);
            ch.terrainVerts.push_back(v10);

            ch.terrainVerts.push_back(v10);
            ch.terrainVerts.push_back(v01);
            ch.terrainVerts.push_back(v11);
        }
    }

    uint32_t seed = voronoi_.seed();

    // 2. Standees, Lights & Enemies for this Chunk
    if (voronoi_.isDungeonMode()) {
        if (!dungeonExplicitDressing_) {
            for (const auto& rm : voronoi_.dungeonRooms()) {
                float cx_rm = 0.5f * (rm.minX + rm.maxX);
                float cz_rm = 0.5f * (rm.minZ + rm.maxZ);

                if (rm.minX + 1.8f >= minX && rm.minX + 1.8f < maxX && rm.minZ + 1.8f >= minZ && rm.minZ + 1.8f < maxZ) {
                    StandeeInstance st{};
                    st.worldPos[0] = rm.minX + 1.8f; st.worldPos[1] = 0.25f; st.worldPos[2] = rm.minZ + 1.8f;
                    st.width = 1.4f; st.height = 1.5f; st.texLayer = InkLayer::StandeeCampfire;
                    st.flags = StandeeFlags::Emissive | StandeeFlags::IdleBob;
                    ch.standees.push_back(st);
                }
                if (rm.maxX - 1.8f >= minX && rm.maxX - 1.8f < maxX && rm.minZ + 1.8f >= minZ && rm.minZ + 1.8f < maxZ) {
                    StandeeInstance st{};
                    st.worldPos[0] = rm.maxX - 1.8f; st.worldPos[1] = 0.25f; st.worldPos[2] = rm.minZ + 1.8f;
                    st.width = 1.4f; st.height = 1.5f; st.texLayer = InkLayer::StandeeCampfire;
                    st.flags = StandeeFlags::Emissive | StandeeFlags::IdleBob;
                    ch.standees.push_back(st);
                }
                if (cx_rm >= minX && cx_rm < maxX && cz_rm >= minZ && cz_rm < maxZ) {
                    PointLight pl{};
                    pl.position[0] = cx_rm; pl.position[1] = 2.2f; pl.position[2] = cz_rm;
                    pl.radius = 16.0f;
                    if (voronoi_.dungeonTheme() == DungeonTheme::SunkenSanctum) {
                        pl.color[0] = 0.45f; pl.color[1] = 0.85f; pl.color[2] = 1.0f; // Cool cyan cistern light
                    } else if (voronoi_.dungeonTheme() == DungeonTheme::MoltenVault) {
                        pl.color[0] = 1.0f; pl.color[1] = 0.48f; pl.color[2] = 0.16f; // Fiery ember light
                    } else if (voronoi_.dungeonTheme() == DungeonTheme::GildedReliquary) {
                        pl.color[0] = 1.0f; pl.color[1] = 0.88f; pl.color[2] = 0.40f; // Bright gilded light
                    } else {
                        pl.color[0] = 1.0f; pl.color[1] = 0.72f; pl.color[2] = 0.36f; // Warm torchlight
                    }
                    pl.intensity = 1.35f;
                    ch.lights.push_back(pl);

                    if (rm.isBossRoom) {
                        bool hasBoss = false;
                        for (const auto& en : enemies_) if (en.isBoss) { hasBoss = true; break; }
                        if (!hasBoss) {
                            EnemyAgent boss{};
                            boss.pos = {cx_rm, 0.25f, cz_rm + 2.0f};
                            boss.homePos = boss.pos;
                            boss.standeeLayer = InkLayer::StandeeMonsterWyvern;
                            boss.name = (voronoi_.dungeonTheme() == DungeonTheme::MoltenVault) ? "The Molten Drake" :
                                        (voronoi_.dungeonTheme() == DungeonTheme::WhisperingCrypt) ? "The Bone Archon" :
                                        (voronoi_.dungeonTheme() == DungeonTheme::GildedReliquary) ? "The Gilded Titan" :
                                        "The Sunken Wyrm";
                            boss.width = 4.6f; boss.height = 4.6f;
                            boss.hp = 220; boss.maxHp = 220;
                            boss.telegraphRadius = 6.2f;
                            boss.telegraphType = TelegraphType::EnemyCircle;
                            boss.isBoss = true;
                            enemies_.push_back(boss);
                        }
                    } else if (rm.id >= 1 && enemies_.size() < 6) {
                        EnemyAgent guard{};
                        guard.pos = {cx_rm, 0.25f, cz_rm};
                        guard.homePos = guard.pos;
                        guard.standeeLayer = (rm.id % 2 == 0) ? InkLayer::StandeeMonsterBandit : InkLayer::StandeeMonsterSlime;
                        guard.name = (voronoi_.dungeonTheme() == DungeonTheme::WhisperingCrypt) ? "Barrow Wight" :
                                     (voronoi_.dungeonTheme() == DungeonTheme::MoltenVault) ? "Magma Slime" :
                                     (rm.id % 2 == 0) ? "Crypt Marauder" : "Rune Slime";
                        guard.hp = 55; guard.maxHp = 55;
                        guard.telegraphRadius = 3.8f;
                        guard.telegraphType = (rm.id % 2 == 0) ? TelegraphType::EnemyCone : TelegraphType::EnemyCircle;
                        enemies_.push_back(guard);
                    }
                }
            }
            for (const auto& p : voronoi_.pois()) {
                if (p.pos.x >= minX && p.pos.x < maxX && p.pos.y >= minZ && p.pos.y < maxZ) {
                    uint16_t layer = InkLayer::StandeeDungeonArch;
                    if (p.category == PoiCategory::StandingStones) layer = InkLayer::StandeeStandingStone;
                    else if (p.category == PoiCategory::SunkenRuin) layer = InkLayer::StandeeTreasureChest;
                    if (p.category != PoiCategory::BossLair) {
                        StandeeInstance st{};
                        st.worldPos[0] = p.pos.x; st.worldPos[1] = 0.25f; st.worldPos[2] = p.pos.y;
                        st.width = 2.6f; st.height = 2.8f; st.texLayer = layer;
                        st.flags = StandeeFlags::PopUpTilt;
                        ch.standees.push_back(st);
                    }
                }
            }
        }
    } else {
        // Overworld POIs in this Chunk
        for (const auto& p : voronoi_.pois()) {
            if (p.pos.x >= minX && p.pos.x < maxX && p.pos.y >= minZ && p.pos.y < maxZ) {
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

                StandeeInstance st{};
                st.worldPos[0] = p.pos.x; st.worldPos[1] = y; st.worldPos[2] = p.pos.y;
                st.width = w; st.height = h; st.texLayer = layer;
                st.flags = StandeeFlags::PopUpTilt;
                st.phaseOffset = std::fmod(std::abs(p.pos.x * 1.37f + p.pos.y * 2.19f), 6.28f);
                ch.standees.push_back(st);

                PointLight pl{};
                pl.position[0] = p.pos.x; pl.position[1] = y + 2.4f; pl.position[2] = p.pos.y - 1.0f;
                pl.radius = 14.0f; pl.color[0] = 1.0f; pl.color[1] = 0.78f; pl.color[2] = 0.42f;
                pl.intensity = 1.25f;
                ch.lights.push_back(pl);

                // Local guardians/bosses near POIs
                bool hasEnemyNear = false;
                for (const auto& en : enemies_) {
                    float edx = en.pos.x - p.pos.x;
                    float edz = en.pos.z - p.pos.y;
                    if (std::sqrt(edx * edx + edz * edz) < 10.0f) { hasEnemyNear = true; break; }
                }
                if (!hasEnemyNear) {
                    if (p.category == PoiCategory::BossLair) {
                        EnemyAgent boss{};
                        boss.pos = {p.pos.x - 2.5f, y, p.pos.y - 2.5f};
                        boss.homePos = boss.pos;
                        boss.standeeLayer = InkLayer::StandeeMonsterWyvern;
                        boss.name = p.name;
                        boss.width = 4.5f; boss.height = 4.5f;
                        boss.hp = 200; boss.maxHp = 200;
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
                        guard.hp = 60; guard.maxHp = 60;
                        guard.telegraphRadius = 3.8f;
                        guard.telegraphType = TelegraphType::EnemyCone;
                        enemies_.push_back(guard);
                    }
                }
            }
        }

        // Procedural Nature Standees (2.5D Trees, Grass Patches, Crag Rocks & Sea Stacks)
        int minLatticeX = static_cast<int>(std::floor(minX / 4.0f)) * 4;
        int maxLatticeX = static_cast<int>(std::ceil(maxX / 4.0f)) * 4;
        int minLatticeZ = static_cast<int>(std::floor(minZ / 4.0f)) * 4;
        int maxLatticeZ = static_cast<int>(std::ceil(maxZ / 4.0f)) * 4;

        // Helper to interpolate precomputed chunk halo samples at (wx, wz) in [minX, maxX) x [minZ, maxZ)
        auto samplePrecomputedGrid = [&](float wx, float wz, float& outMoisture) noexcept -> ChartFieldSample {
            float fx = std::clamp(wx - minX, 0.0f, static_cast<float>(kGridN));
            float fz = std::clamp(wz - minZ, 0.0f, static_cast<float>(kGridN));
            int ix = std::clamp(static_cast<int>(fx), 0, kGridN - 1);
            int iz = std::clamp(static_cast<int>(fz), 0, kGridN - 1);
            float tx = fx - static_cast<float>(ix);
            float tz = fz - static_cast<float>(iz);

            int lx0 = ix + 1;
            int lz0 = iz + 1;
            int lxN = (tx < 0.5f) ? lx0 : (lx0 + 1);
            int lzN = (tz < 0.5f) ? lz0 : (lz0 + 1);

            size_t i00 = static_cast<size_t>(lz0) * kHaloDim + lx0;
            size_t i10 = static_cast<size_t>(lz0) * kHaloDim + (lx0 + 1);
            size_t i01 = static_cast<size_t>(lz0 + 1) * kHaloDim + lx0;
            size_t i11 = static_cast<size_t>(lz0 + 1) * kHaloDim + (lx0 + 1);
            size_t iNear = static_cast<size_t>(lzN) * kHaloDim + lxN;

            ChartFieldSample s = samples[iNear];
            auto bilerp = [&](float v00, float v10, float v01, float v11) noexcept {
                float a = v00 + (v10 - v00) * tx;
                float b = v01 + (v11 - v01) * tx;
                return a + (b - a) * tz;
            };
            s.elevation = bilerp(samples[i00].elevation, samples[i10].elevation, samples[i01].elevation, samples[i11].elevation);
            s.sdfCoast  = bilerp(samples[i00].sdfCoast,  samples[i10].sdfCoast,  samples[i01].sdfCoast,  samples[i11].sdfCoast);
            s.sdfRoad   = bilerp(samples[i00].sdfRoad,   samples[i10].sdfRoad,   samples[i01].sdfRoad,   samples[i11].sdfRoad);
            s.sdfRiver  = bilerp(samples[i00].sdfRiver,  samples[i10].sdfRiver,  samples[i01].sdfRiver,  samples[i11].sdfRiver);
            outMoisture = bilerp(haloM[i00], haloM[i10], haloM[i01], haloM[i11]);
            return s;
        };

        for (int gz = minLatticeZ; gz < maxLatticeZ; gz += 4) {
            for (int gx = minLatticeX; gx < maxLatticeX; gx += 4) {
                uint32_t h = hashPos(gx, gz, seed);
                float jx = (static_cast<float>(h & 0xFFu) / 255.0f - 0.5f) * 3.1f;
                float jz = (static_cast<float>((h >> 8u) & 0xFFu) / 255.0f - 0.5f) * 3.1f;
                float wx = static_cast<float>(gx) + jx;
                float wz = static_cast<float>(gz) + jz;

                if (wx < minX || wx >= maxX || wz < minZ || wz >= maxZ) continue;

                float localMoisture = 0.5f;
                ChartFieldSample s = samplePrecomputedGrid(wx, wz, localMoisture);

                // Sorcery!-style hand-inked waypoint pennant flags along the road verge
                if (s.sdfRoad >= 1.2f && s.sdfRoad <= 2.3f && (h % 31u == 0u) && s.sdfRiver > s.riverWidth + 2.0f &&
                    s.sdfCoast > 2.5f && s.elevation > 0.4f && (s.flags & ChartFlags::IsSettlement) == 0u) {
                    uint32_t flipFlag = ((h >> 16u) & 1u) ? StandeeFlags::FlipX : 0u;
                    StandeeInstance sign{};
                    sign.worldPos[0] = wx; sign.worldPos[1] = s.elevation; sign.worldPos[2] = wz;
                    sign.width = 1.8f; sign.height = 2.2f;
                    sign.texLayer = InkLayer::StandeeSignpost;
                    sign.flags = StandeeFlags::WindSway | StandeeFlags::PopUpTilt | flipFlag;
                    sign.phaseOffset = std::fmod(std::abs(wx * 1.51f + wz * 2.37f), 6.28f);
                    ch.standees.push_back(sign);
                    continue;
                }

                if (s.sdfRoad < 2.4f || s.sdfRiver < s.riverWidth + 1.2f || (s.flags & ChartFlags::IsSettlement) != 0u) {
                    continue;
                }

                uint32_t flipFlag = ((h >> 16u) & 1u) ? StandeeFlags::FlipX : 0u;

                if ((s.flags & ChartFlags::IsWater) != 0u) {
                    // Offshore 2.5D faceted basalt sea stacks & coastal reef rocks protruding from water
                    if (s.sdfCoast > -12.0f && s.sdfCoast < -1.8f && (h % 13u == 0u)) {
                        float scale = 2.6f + static_cast<float>((h >> 4u) & 7u) * 0.28f;
                        uint16_t rLayer = ((h >> 7u) % 3u == 0u) ? InkLayer::StandeeRockPillar : InkLayer::StandeeSeaStack;
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = -0.08f; st.worldPos[2] = wz;
                        st.width = scale * 1.05f; st.height = scale * 1.15f;
                        st.texLayer = rLayer;
                        st.flags = StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    }
                    continue;
                }

                // Land elevation > 5.5m is already represented by the 3D terrain mesh!
                // Only place occasional 2.5D rocky crag outcrops on rocky slopes/badlands (no redundant 2D mountain bitmaps)
                if (s.biomePrimary == BiomeId::AlpinePeak || s.biomePrimary == BiomeId::BadlandsCanyon || s.elevation > 5.5f) {
                    if (h % 11u == 0u && s.elevation < 7.2f) {
                        uint16_t rLayer = ((h >> 4u) & 1u) ? InkLayer::StandeeRockOutcrop : InkLayer::StandeeRockPillar;
                        float scale = 2.0f + static_cast<float>(h & 3u) * 0.30f;
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation - 0.08f; st.worldPos[2] = wz;
                        st.width = scale * 1.12f; st.height = scale;
                        st.texLayer = rLayer;
                        st.flags = StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    }
                } else if (s.biomePrimary == BiomeId::DeciduousWoodland || s.biomePrimary == BiomeId::LushMeadow || s.biomePrimary == BiomeId::SaltMarsh) {
                    uint32_t treeMod = (s.biomePrimary == BiomeId::DeciduousWoodland) ? 3u : 10u;
                    if (h % treeMod == 0u && s.sdfCoast > 1.8f && s.biomePrimary != BiomeId::SaltMarsh) {
                        float scale = 2.8f + static_cast<float>(h & 3u) * 0.32f;
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                        st.width = scale * 1.15f; st.height = scale * 1.12f;
                        st.texLayer = InkLayer::StandeeForestOak;
                        st.flags = StandeeFlags::WindSway | StandeeFlags::PartableCanopy | StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 2u == 0u && s.sdfCoast > 1.2f) {
                        // Realms-style 2.5D lush grass patches over grassy terrain (skips shadow map for peak perf)
                        float gScale = 1.15f + static_cast<float>((h >> 3u) & 3u) * 0.22f;
                        StandeeInstance gs{};
                        gs.worldPos[0] = wx; gs.worldPos[1] = s.elevation; gs.worldPos[2] = wz;
                        gs.width = gScale * 1.25f; gs.height = gScale * 0.95f;
                        gs.texLayer = InkLayer::StandeeGrassPatch;
                        gs.flags = StandeeFlags::WindSway | StandeeFlags::NoShadow | flipFlag;
                        gs.phaseOffset = std::fmod(std::abs(wx * 2.13f + wz * 1.79f), 6.28f);
                        ch.standees.push_back(gs);
                    }
                } else if (s.biomePrimary == BiomeId::TaigaPines) {
                    if (h % 3u == 0u) {
                        float scale = 2.9f + static_cast<float>(h & 3u) * 0.30f;
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                        st.width = scale; st.height = scale * 1.18f;
                        st.texLayer = InkLayer::StandeeForestPine;
                        st.flags = StandeeFlags::WindSway | StandeeFlags::PartableCanopy | StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 5u == 0u) {
                        StandeeInstance gs{};
                        gs.worldPos[0] = wx; gs.worldPos[1] = s.elevation; gs.worldPos[2] = wz;
                        gs.width = 1.3f; gs.height = 1.0f;
                        gs.texLayer = InkLayer::StandeeGrassPatch;
                        gs.flags = StandeeFlags::WindSway | StandeeFlags::NoShadow | flipFlag;
                        gs.phaseOffset = std::fmod(std::abs(wx * 2.13f + wz * 1.79f), 6.28f);
                        ch.standees.push_back(gs);
                    }
                } else if (s.biomePrimary == BiomeId::SandyBeach && (h % 11u == 0u)) {
                    StandeeInstance st{};
                    st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                    st.width = 2.5f; st.height = 2.8f;
                    st.texLayer = InkLayer::StandeeForestPalm;
                    st.flags = StandeeFlags::WindSway | StandeeFlags::PartableCanopy | StandeeFlags::PopUpTilt | flipFlag;
                    st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                    ch.standees.push_back(st);
                } else if (s.biomePrimary == BiomeId::AridSteppe) {
                    if (localMoisture > 0.35f && (h % 3u == 0u)) {
                        // Oasis date palms swaying above the emerald pool
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                        st.width = 2.6f + static_cast<float>(h & 3u) * 0.28f;
                        st.height = st.width * 1.15f;
                        st.texLayer = InkLayer::StandeeForestPalm;
                        st.flags = StandeeFlags::WindSway | StandeeFlags::PartableCanopy | StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 13u == 0u) {
                        // Sun-baked monoliths & sandstone pillars across the arid plains
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation - 0.05f; st.worldPos[2] = wz;
                        st.width = 2.2f; st.height = 2.5f;
                        st.texLayer = ((h >> 2u) & 1u) ? InkLayer::StandeeStandingStone : InkLayer::StandeeRockPillar;
                        st.flags = StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 17u == 0u) {
                        // Arid desert scrub / dried grass tuft
                        StandeeInstance gs{};
                        gs.worldPos[0] = wx; gs.worldPos[1] = s.elevation; gs.worldPos[2] = wz;
                        gs.width = 1.25f; gs.height = 0.95f;
                        gs.texLayer = InkLayer::StandeeGrassPatch;
                        gs.flags = StandeeFlags::WindSway | StandeeFlags::NoShadow | flipFlag;
                        gs.phaseOffset = std::fmod(std::abs(wx * 2.13f + wz * 1.79f), 6.28f);
                        ch.standees.push_back(gs);
                    }
                } else if (s.biomePrimary == BiomeId::SnowyTundra) {
                    if (h % 4u == 0u) {
                        // Frost-laden boreal pine stand
                        float scale = 2.7f + static_cast<float>(h & 3u) * 0.28f;
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                        st.width = scale; st.height = scale * 1.18f;
                        st.texLayer = InkLayer::StandeeForestPine;
                        st.flags = StandeeFlags::WindSway | StandeeFlags::PartableCanopy | StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 7u == 0u) {
                        // Frost-rimed boulder / crag outcrop
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation - 0.05f; st.worldPos[2] = wz;
                        st.width = 2.0f; st.height = 2.2f;
                        st.texLayer = ((h >> 2u) & 1u) ? InkLayer::StandeeRockOutcrop : InkLayer::StandeeRockPillar;
                        st.flags = StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 11u == 0u) {
                        // Ancient frost monolith / standing stone
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                        st.width = 1.8f; st.height = 2.6f;
                        st.texLayer = InkLayer::StandeeStandingStone;
                        st.flags = StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 13u == 0u) {
                        // Winter grass tuft / tundra scrub
                        StandeeInstance gs{};
                        gs.worldPos[0] = wx; gs.worldPos[1] = s.elevation; gs.worldPos[2] = wz;
                        gs.width = 1.2f; gs.height = 0.9f;
                        gs.texLayer = InkLayer::StandeeGrassPatch;
                        gs.flags = StandeeFlags::WindSway | StandeeFlags::NoShadow | flipFlag;
                        gs.phaseOffset = std::fmod(std::abs(wx * 2.13f + wz * 1.79f), 6.28f);
                        ch.standees.push_back(gs);
                    }
                } else if (s.biomePrimary == BiomeId::GlacialIce) {
                    if (h % 9u == 0u) {
                        // Jagged blue ice spire / rock pillar
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                        st.width = 2.2f + static_cast<float>(h & 3u) * 0.35f;
                        st.height = st.width * 1.35f;
                        st.texLayer = InkLayer::StandeeRockPillar;
                        st.flags = StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    } else if (h % 17u == 0u) {
                        // Mystic frozen standing stone
                        StandeeInstance st{};
                        st.worldPos[0] = wx; st.worldPos[1] = s.elevation; st.worldPos[2] = wz;
                        st.width = 1.9f; st.height = 2.7f;
                        st.texLayer = InkLayer::StandeeStandingStone;
                        st.flags = StandeeFlags::PopUpTilt | flipFlag;
                        st.phaseOffset = std::fmod(std::abs(wx * 1.37f + wz * 2.19f), 6.28f);
                        ch.standees.push_back(st);
                    }
                }

                // Scatter wandering slimes and desert creatures in wilderness
                if (s.sdfCoast > 3.5f && s.elevation < 4.2f && (h % 59u == 0u) && enemies_.size() < 24) {
                    EnemyAgent slime{};
                    slime.pos = {wx, s.elevation, wz};
                    slime.homePos = slime.pos;
                    slime.standeeLayer = InkLayer::StandeeMonsterSlime;
                    slime.name = (s.biomePrimary == BiomeId::AridSteppe || s.biomePrimary == BiomeId::BadlandsCanyon) ? "Dune Scuttler" :
                                 (s.biomePrimary == BiomeId::SnowyTundra || s.biomePrimary == BiomeId::GlacialIce) ? "Frost Slime" : "Wild Ink Slime";
                    slime.width = 1.9f; slime.height = 1.9f;
                    slime.hp = 40; slime.maxHp = 40;
                    slime.telegraphRadius = 3.2f;
                    slime.telegraphType = TelegraphType::EnemyCircle;
                    enemies_.push_back(slime);
                }
            }
        }
    }

    // Cartographic 3D Curved Ground Banners (Landmark POIs, Regional Titles & Custom Banners)
    auto bannerIntersects = [&](const CurvedMapBanner& b) {
        float bMinX = std::min({b.p0.x, b.p1.x, b.p2.x}) - 20.0f;
        float bMaxX = std::max({b.p0.x, b.p1.x, b.p2.x}) + 20.0f;
        float bMinZ = std::min({b.p0.y, b.p1.y, b.p2.y}) - 20.0f;
        float bMaxZ = std::max({b.p0.y, b.p1.y, b.p2.y}) + 20.0f;
        return (bMaxX >= minX && bMinX <= maxX && bMaxZ >= minZ && bMinZ <= maxZ);
    };

    if (voronoi_.isDungeonMode()) {
        // Watabou One-Page Dungeon room label banners on flagstone floors
        for (const auto& rm : voronoi_.dungeonRooms()) {
            if (rm.label.empty()) continue;
            float cx_rm = (rm.minX + rm.maxX) * 0.5f;
            float cz_rm = (rm.minZ + rm.maxZ) * 0.5f;
            float rSpan = std::max(6.0f, static_cast<float>(rm.label.size()) * 0.95f);
            if (cx_rm >= minX - rSpan && cx_rm <= maxX + rSpan &&
                cz_rm >= minZ - 6.0f && cz_rm <= maxZ + 6.0f) {
                CurvedMapBanner rmBanner{};
                rmBanner.text = rm.label;
                rmBanner.p0 = {cx_rm - rSpan, cz_rm - 1.2f};
                rmBanner.p1 = {cx_rm,         cz_rm - 2.4f};
                rmBanner.p2 = {cx_rm + rSpan, cz_rm - 1.2f};
                rmBanner.letterSize = 1.9f;
                rmBanner.isRegionHeader = false;
                emitBannerToChunk(rmBanner, ch.bannerVerts, minX, maxX, minZ, maxZ);
            }
        }
    } else {
        // 1. Regional macro calligraphic banners (authored & procedural from Voronoi Atlas)
        for (const auto& b : voronoi_.banners()) {
            if (bannerIntersects(b)) {
                emitBannerToChunk(b, ch.bannerVerts, minX, maxX, minZ, maxZ);
            }
        }

        // 2. Custom scripted banners (from chart.add_banner / chart.add_curved_banner)
        for (const auto& b : customBanners_) {
            if (bannerIntersects(b)) {
                emitBannerToChunk(b, ch.bannerVerts, minX, maxX, minZ, maxZ);
            }
        }

        // 3. Local landmark / settlement POI ribbons (fitted to terrain with cliff drop avoidance & de-confliction)
        for (const auto& p : voronoi_.pois()) {
            if (p.name.empty()) continue;
            float halfSpan = std::max(8.0f, static_cast<float>(p.name.size()) * 1.05f);
            if (p.pos.x >= minX - halfSpan - 20.0f && p.pos.x <= maxX + halfSpan + 20.0f &&
                p.pos.y >= minZ - 25.0f && p.pos.y <= maxZ + 25.0f) {
                bool isWaterPoi = (p.category == PoiCategory::SunkenRuin || p.category == PoiCategory::PortHarbor);
                auto fit = CartographicPathText::fitBanner(
                    p.name,
                    p.pos,
                    2.2f, // Cat Quest hero scale (~2.2m)
                    voronoi_,
                    voronoi_.banners(),
                    isWaterPoi,
                    /*isRegionHeader=*/false
                );
                if (bannerIntersects(fit.banner)) {
                    emitBannerToChunk(fit.banner, ch.bannerVerts, minX, maxX, minZ, maxZ);
                }
            }
        }
    }

    return ch;
}

void ChartWorld::assembleActiveMesh() {
    worldVerts_.clear();
    staticStandees_.clear();
    staticChunkLights_.clear();
    mapSurfaceTriCount_ = 0;
    bannerGlyphTriCount_ = 0;

    constexpr size_t kChunksCount = (kChunkRadius * 2 + 1) * (kChunkRadius * 2 + 1);
    worldVerts_.reserve(kChunksCount * (32 * 32 * 6 + 256));

    for (int dz = -kChunkRadius; dz <= kChunkRadius; ++dz) {
        for (int dx = -kChunkRadius; dx <= kChunkRadius; ++dx) {
            int cx = activeChunkX_ + dx;
            int cz = activeChunkZ_ + dz;
            auto it = chunkCache_.find(packChunkKey(cx, cz));
            if (it == chunkCache_.end()) continue;

            const auto& ch = it->second;
            worldVerts_.insert(worldVerts_.end(), ch.terrainVerts.begin(), ch.terrainVerts.end());
            mapSurfaceTriCount_ += static_cast<uint32_t>(ch.terrainVerts.size() / 3);

            worldVerts_.insert(worldVerts_.end(), ch.bannerVerts.begin(), ch.bannerVerts.end());
            bannerGlyphTriCount_ += static_cast<uint32_t>(ch.bannerVerts.size() / 3);

            staticStandees_.insert(staticStandees_.end(), ch.standees.begin(), ch.standees.end());
            staticChunkLights_.insert(staticChunkLights_.end(), ch.lights.begin(), ch.lights.end());
        }
    }

    // Append script-added custom standees
    staticStandees_.insert(staticStandees_.end(), customStandees_.begin(), customStandees_.end());
    activeStandees_.clear();
    activeStandees_.reserve(staticStandees_.size() + 32);
    activeStandees_.insert(activeStandees_.end(), staticStandees_.begin(), staticStandees_.end());

    // Prune distant chunks if cache grows large
    if (chunkCache_.size() > 100) {
        for (auto it = chunkCache_.begin(); it != chunkCache_.end(); ) {
            int cDistX = std::abs(it->second.cx - activeChunkX_);
            int cDistZ = std::abs(it->second.cz - activeChunkZ_);
            if (cDistX > kChunkRadius + 3 || cDistZ > kChunkRadius + 3) {
                it = chunkCache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    buildDynamicStandeesAndTelegraphs();
    meshDirty_ = true;
}

void ChartWorld::servicePendingChunkBakes() {
    if (pendingBakeChunks_.empty()) return;

    // Bake at most 1 chunk per frame to keep frame time < 16ms (zero hitching)
    auto [cx, cz] = pendingBakeChunks_.back();
    pendingBakeChunks_.pop_back();

    uint64_t key = packChunkKey(cx, cz);
    if (chunkCache_.find(key) == chunkCache_.end()) {
        chunkCache_[key] = bakeChunk(cx, cz);
    }

    // Once all missing chunks for the new window are baked, activate and upload
    if (pendingBakeChunks_.empty() && (activeChunkX_ != targetChunkX_ || activeChunkZ_ != targetChunkZ_)) {
        activeChunkX_ = targetChunkX_;
        activeChunkZ_ = targetChunkZ_;
        gridCenter_ = {static_cast<float>(activeChunkX_) * kChunkSize + kChunkSize * 0.5f,
                       static_cast<float>(activeChunkZ_) * kChunkSize + kChunkSize * 0.5f};
        assembleActiveMesh();
    }
}

void ChartWorld::updateChunkStreaming(float heroX, float heroZ, bool forceRebuild) {
    int curChunkX = static_cast<int>(std::floor(heroX / kChunkSize));
    int curChunkZ = static_cast<int>(std::floor(heroZ / kChunkSize));

    if (!forceRebuild && curChunkX == activeChunkX_ && curChunkZ == activeChunkZ_) {
        return; // Zero work!
    }

    if (forceRebuild) {
        pendingBakeChunks_.clear();
        activeChunkX_ = curChunkX;
        activeChunkZ_ = curChunkZ;
        targetChunkX_ = curChunkX;
        targetChunkZ_ = curChunkZ;
        gridCenter_ = {static_cast<float>(curChunkX) * kChunkSize + kChunkSize * 0.5f,
                       static_cast<float>(curChunkZ) * kChunkSize + kChunkSize * 0.5f};

        for (int dz = -kChunkRadius; dz <= kChunkRadius; ++dz) {
            for (int dx = -kChunkRadius; dx <= kChunkRadius; ++dx) {
                int cx = curChunkX + dx;
                int cz = curChunkZ + dz;
                uint64_t key = packChunkKey(cx, cz);
                if (chunkCache_.find(key) == chunkCache_.end()) {
                    chunkCache_[key] = bakeChunk(cx, cz);
                }
            }
        }
        assembleActiveMesh();
        return;
    }

    // Check what chunks are missing for (curChunkX, curChunkZ)
    std::vector<std::pair<int, int>> missing;
    for (int dz = -kChunkRadius; dz <= kChunkRadius; ++dz) {
        for (int dx = -kChunkRadius; dx <= kChunkRadius; ++dx) {
            int cx = curChunkX + dx;
            int cz = curChunkZ + dz;
            uint64_t key = packChunkKey(cx, cz);
            if (chunkCache_.find(key) == chunkCache_.end()) {
                missing.emplace_back(cx, cz);
            }
        }
    }

    if (missing.empty()) {
        // All chunks already cached! Instant seamless transition!
        pendingBakeChunks_.clear();
        activeChunkX_ = curChunkX;
        activeChunkZ_ = curChunkZ;
        targetChunkX_ = curChunkX;
        targetChunkZ_ = curChunkZ;
        gridCenter_ = {static_cast<float>(curChunkX) * kChunkSize + kChunkSize * 0.5f,
                       static_cast<float>(curChunkZ) * kChunkSize + kChunkSize * 0.5f};
        assembleActiveMesh();
    } else {
        // Queue missing chunks to be amortized 1 chunk per frame
        targetChunkX_ = curChunkX;
        targetChunkZ_ = curChunkZ;
        pendingBakeChunks_ = std::move(missing);
        // Bake first chunk immediately
        servicePendingChunkBakes();
    }
}

ChartStats ChartWorld::rebuildWorldGeometry(float centerWorldX, float centerWorldZ) {
    if (worldBanners_.empty()) {
        rebuildWorldBanners();
    }
    updateChunkStreaming(centerWorldX, centerWorldZ, /*forceRebuild=*/true);

    ChartStats stats{};
    stats.atlasGenTimeMs       = atlasGenTimeMs_;
    stats.mapSurfaceTriangles  = mapSurfaceTriCount_;
    stats.bannerGlyphTriangles = bannerGlyphTriCount_;
    stats.standeeCount         = static_cast<uint32_t>(activeStandees_.size());
    stats.telegraphCount       = static_cast<uint32_t>(activeTelegraphs_.size());
    stats.pointLightCount      = static_cast<uint32_t>(pointLights_.size());
    return stats;
}

void ChartWorld::fillDiscoveryGrid(int centerCx, int centerCz, float outGrid[16][4]) const noexcept {
    std::memset(outGrid, 0, sizeof(float) * 64);
    if (voronoi_.isDungeonMode()) {
        const auto& rooms = voronoi_.dungeonRooms();
        const auto& corrs = voronoi_.dungeonCorridors();
        const auto& pools = voronoi_.dungeonWaterPools();

        // outGrid[0..5]: Rooms (minX, minZ, maxX, maxZ)
        for (size_t i = 0; i < rooms.size() && i < 6; ++i) {
            outGrid[i][0] = rooms[i].minX;
            outGrid[i][1] = rooms[i].minZ;
            outGrid[i][2] = rooms[i].maxX;
            outGrid[i][3] = rooms[i].maxZ;
        }

        // outGrid[6..10]: Corridors (p0.x, p0.y, p1.x, p1.y)
        for (size_t i = 0; i < corrs.size() && i < 5; ++i) {
            outGrid[6 + i][0] = corrs[i].p0.x;
            outGrid[6 + i][1] = corrs[i].p0.y;
            outGrid[6 + i][2] = corrs[i].p1.x;
            outGrid[6 + i][3] = corrs[i].p1.y;
        }

        // outGrid[11]: Counts (roomCount, corrCount, poolCount, 0.0)
        outGrid[11][0] = static_cast<float>(std::min<size_t>(rooms.size(), 6));
        outGrid[11][1] = static_cast<float>(std::min<size_t>(corrs.size(), 5));
        outGrid[11][2] = static_cast<float>(std::min<size_t>(pools.size(), 4));
        outGrid[11][3] = 0.0f;

        // outGrid[12..15]: Water pools (minX, minZ, maxX, maxZ)
        for (size_t i = 0; i < pools.size() && i < 4; ++i) {
            outGrid[12 + i][0] = pools[i].minX;
            outGrid[12 + i][1] = pools[i].minZ;
            outGrid[12 + i][2] = pools[i].maxX;
            outGrid[12 + i][3] = pools[i].maxZ;
        }
        return;
    }

    constexpr int stride = kChunkRadius * 2 + 1; // 5
    for (int dz = -kChunkRadius; dz <= kChunkRadius; ++dz) {
        for (int dx = -kChunkRadius; dx <= kChunkRadius; ++dx) {
            int cx = centerCx + dx;
            int cz = centerCz + dz;
            int slot = (dz + kChunkRadius) * stride + (dx + kChunkRadius);
            uint64_t key = packChunkKey(cx, cz);
            float disc = 0.0f;
            auto it = chunkCache_.find(key);
            if (it != chunkCache_.end()) {
                disc = it->second.discovery;
            }
            outGrid[slot / 4][slot % 4] = disc;
        }
    }
}

uint32_t ChartWorld::discoveredChunkCount() const noexcept {
    uint32_t count = 0;
    for (const auto& kv : chunkCache_) {
        if (kv.second.discovery >= 0.5f) {
            count++;
        }
    }
    return count;
}

void ChartWorld::moveHero(float moveX, float moveZ, bool sprintOrDodge, float dt) {
    if (hero_.dodgeCooldown > 0.0f) hero_.dodgeCooldown = std::max(0.0f, hero_.dodgeCooldown - dt);
    if (hero_.dodgeTimer > 0.0f)    hero_.dodgeTimer    = std::max(0.0f, hero_.dodgeTimer - dt);

    float len = std::sqrt(moveX * moveX + moveZ * moveZ);
    bool isMoving = (len > 1e-4f);
    if (isMoving) {
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

    ChartFieldSample currentSample = voronoi_.sampleAt(hero_.pos.x, hero_.pos.z);
    bool inWater = (currentSample.sdfCoast < -0.45f && (currentSample.flags & ChartFlags::IsBridge) == 0u && !voronoi_.isDungeonMode());
    hero_.isSailing = inWater;

    float speed = 10.5f;

    if (hero_.isSailing) {
        // Water movement: slower for shore lines and increasingly faster to full speed as player ventures into deeper waters
        float shoreDist = std::max(0.0f, -currentSample.sdfCoast);
        // Shallow waters (0m to ~10m from shore) slow movement; deep waters scale up to full 14.5f speed
        // At the coastal edge, speed is slowed by half (0.25f multiplier) and smoothly ramps to full speed in deeper waters
        float depthFactor = std::clamp(shoreDist / 9.5f, 0.0f, 1.0f);
        float waterSpeedMultiplier = std::lerp(0.25f, 1.0f, depthFactor);
        speed = 14.5f * waterSpeedMultiplier;

        hero_.hopTimer = 0.0f;
        hero_.hopArc = 0.0f;
    } else {
        // Land movement: Cat Quest rhythmic hopping locomotion!
        // Mimics the ~6 times per second hop pulse for bouncy upward hops and pulsed steps
        constexpr float kHopFreq = 5.8f; // ~6 hops per second

        if (isMoving) {
            hero_.hopTimer += dt * kHopFreq * 6.2831853f;
            float hopSine = std::sin(hero_.hopTimer);
            hero_.hopArc = std::max(0.0f, hopSine);

            // Forward velocity surge during airborne hop, brief deceleration on touchdown
            float movePulse = 0.35f + 1.25f * (hero_.hopArc * hero_.hopArc);
            speed = 10.5f * movePulse;
        } else {
            hero_.hopArc = std::max(0.0f, hero_.hopArc - dt * 8.0f);
            hero_.hopTimer = 0.0f;
            speed = 10.5f;
        }
    }

    if (hero_.dodgeTimer > 0.0f) {
        speed *= 2.35f;
    } else if (sprintOrDodge) {
        speed *= 1.55f;
    }

    float nextX = hero_.pos.x + moveX * speed * dt;
    float nextZ = hero_.pos.z + moveZ * speed * dt;
    if (voronoi_.isDungeonMode()) {
        nextX = std::clamp(nextX, -68.0f, 68.0f);
        nextZ = std::clamp(nextZ, -68.0f, 88.0f);
    }
    ChartFieldSample s = (nextX == hero_.pos.x && nextZ == hero_.pos.z)
        ? currentSample
        : voronoi_.sampleAt(nextX, nextZ);

    if (voronoi_.isDungeonMode()) {
        // Collide against hatched One-Page Dungeon walls with comfortable boundary padding
        if (s.sdfCoast > 0.30f && s.biomePrimary != BiomeId::DungeonHatchedWall) {
            hero_.pos.x = nextX;
            hero_.pos.z = nextZ;
            hero_.pos.y = std::max(0.0f, s.elevation);
        }
        // Continuous chunk streaming in dungeon mode so surrounding dungeon chunks always extend 64m north!
        updateChunkStreaming(hero_.pos.x, hero_.pos.z, false);
    } else {
        hero_.pos.x = nextX;
        hero_.pos.z = nextZ;
        hero_.pos.y = std::max(0.0f, s.elevation);
        // Seamless Cat Quest Land <-> Naval Ship transition at the shoreline!
        hero_.isSailing = (s.sdfCoast < -0.45f && (s.flags & ChartFlags::IsBridge) == 0u);

        // Infinite chunk-based streaming: seamless, zero-stutter horizon updates
        updateChunkStreaming(hero_.pos.x, hero_.pos.z, false);
    }

    // Update active POI region banner when approaching landmarks
    float bestPoiDist = 18.0f;
    const ChartPoi* nearestPoi = nullptr;
    for (const auto& p : voronoi_.pois()) {
        float dx = hero_.pos.x - p.pos.x;
        if (std::abs(dx) >= bestPoiDist) continue;
        float dz = hero_.pos.z - p.pos.y;
        if (std::abs(dz) >= bestPoiDist) continue;
        float d = std::sqrt(dx * dx + dz * dz);
        if (d < bestPoiDist) {
            bestPoiDist = d;
            nearestPoi = &p;
        }
    }
    if (nearestPoi) {
        if (hero_.activeRegionName != nearestPoi->name) {
            hero_.activeRegionName = nearestPoi->name;
            hero_.activeRegionSub  = nearestPoi->subtitle;
            triggerLocaleDiscovery(nearestPoi->name, nearestPoi->subtitle);
        }
    } else if (voronoi_.preset() == AtlasPreset::ContinentalPangaea) {
        std::string provName = "THE IMPERIAL CROSSROADS";
        std::string provSub  = "Heart of Pangaea";
        float rHeroSq = hero_.pos.x * hero_.pos.x + hero_.pos.z * hero_.pos.z;
        if (rHeroSq > 250.0f * 250.0f) {
            if ((s.flags & ChartFlags::IsWater) != 0u) {
                provName = "THE BOUNDLESS OCEAN";
                provSub  = "Uncharted Waters";
            } else {
                BiomeId b = s.biomePrimary;
                if (b == BiomeId::AlpinePeak || b == BiomeId::TaigaPines) {
                    provName = "THE NORTHERN WILDERNESS";
                    provSub  = "Untamed Frontier";
                } else if (b == BiomeId::AridSteppe || b == BiomeId::BadlandsCanyon) {
                    provName = "THE OUTER DESERT";
                    provSub  = "Endless Dunes";
                } else {
                    provName = "THE OVERLAND FRONTIER";
                    provSub  = "Uncharted Lands";
                }
            }
        } else if (hero_.pos.z > 85.0f && hero_.pos.x < -35.0f) {
            provName = "THE FROSTFELL REACH";
            provSub  = (s.biomePrimary == BiomeId::GlacialIce) ? "Blue Crevasse Glacier" : "Snowy Tundra & Ice Horns";
        } else if (hero_.pos.z > 85.0f) {
            provName = "THE REALM OF SILVERVALE";
            provSub  = "Highland Mountains";
        } else if (hero_.pos.x > 85.0f) {
            provName = "THE SUNSCORCHED EXPANSE";
            provSub  = "Arid Wastes";
        } else if (hero_.pos.x < -85.0f) {
            provName = "KILDLAND";
            provSub  = "The Perilous Shores";
        } else if (hero_.pos.z < -85.0f) {
            provName = "THE SHATTERED SEA";
            provSub  = "Corsair's Atoll";
        }
        if (hero_.activeRegionName != provName) {
            hero_.activeRegionName = provName;
            hero_.activeRegionSub  = provSub;
            triggerLocaleDiscovery(provName, provSub);
        }
    } else if (voronoi_.preset() == AtlasPreset::FrostfellPeaks) {
        std::string provName = "RIMEVALE TUNDRA";
        std::string provSub  = "The Frostfell Reach";
        if (s.elevation > 5.5f) {
            provName = "FROSTFELL PEAKS";
            provSub  = "Glacial Summit Horns";
        } else if (s.biomePrimary == BiomeId::GlacialIce) {
            provName = "BLUE CREVASSE GLACIER";
            provSub  = "Ancient Ice Shelf";
        }
        if (hero_.activeRegionName != provName) {
            hero_.activeRegionName = provName;
            hero_.activeRegionSub  = provSub;
            triggerLocaleDiscovery(provName, provSub);
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
        switch (overworldReturnPreset_) {
            case AtlasPreset::PerilousPeninsula:
                hero_.activeRegionName = "LOWMILLBURG";
                hero_.activeRegionSub  = "Kildland Realm";
                break;
            case AtlasPreset::ShatteredArchipelago:
                hero_.activeRegionName = "CORSAIR'S HAVEN";
                hero_.activeRegionSub  = "The Shattered Sea";
                break;
            case AtlasPreset::HighlandKingdom:
                hero_.activeRegionName = "HIGHGUARD KEEP";
                hero_.activeRegionSub  = "Silvervale Realm";
                break;
            case AtlasPreset::SunscorchedWastes:
                hero_.activeRegionName = "AL-ZAHRA OASIS";
                hero_.activeRegionSub  = "Sunscorched Wastes";
                break;
            case AtlasPreset::FrostfellPeaks:
                hero_.activeRegionName = "RIMEVALE HAMLET";
                hero_.activeRegionSub  = "The Frostfell Reach";
                break;
            case AtlasPreset::ContinentalPangaea:
                if (retPos.z > 85.0f && retPos.x < -35.0f) {
                    hero_.activeRegionName = "THE FROSTFELL REACH";
                    hero_.activeRegionSub  = "Glacial Expanse & Tundra";
                } else if (retPos.z > 85.0f) {
                    hero_.activeRegionName = "THE REALM OF SILVERVALE";
                    hero_.activeRegionSub  = "Highland Mountains";
                } else if (retPos.x > 85.0f) {
                    hero_.activeRegionName = "THE SUNSCORCHED EXPANSE";
                    hero_.activeRegionSub  = "Arid Wastes";
                } else if (retPos.x < -85.0f) {
                    hero_.activeRegionName = "KILDLAND";
                    hero_.activeRegionSub  = "The Perilous Shores";
                } else if (retPos.z < -85.0f) {
                    hero_.activeRegionName = "THE SHATTERED SEA";
                    hero_.activeRegionSub  = "Corsair's Atoll";
                } else {
                    hero_.activeRegionName = "THE IMPERIAL CROSSROADS";
                    hero_.activeRegionSub  = "Heart of Pangaea";
                }
                break;
            default:
                break;
        }
        triggerLocaleDiscovery(hero_.activeRegionName, hero_.activeRegionSub);
        rebuildWorldGeometry(retPos.x, retPos.z);
        return true;
    }

    for (const auto& p : voronoi_.pois()) {
        if (p.category == PoiCategory::DungeonPortal || p.category == PoiCategory::SunkenRuin) {
            float dx = hero_.pos.x - p.pos.x;
            float dz = hero_.pos.z - p.pos.y;
            if (std::sqrt(dx * dx + dz * dz) < 6.5f) {
                overworldReturnPos_ = hero_.pos;
                overworldReturnSeed_ = voronoi_.seed();
                overworldReturnPreset_ = voronoi_.preset();
                buildOnePageDungeon(p.seed);
                rebuildWorldGeometry(hero_.pos.x, hero_.pos.z);
                return true;
            }
        }
    }
    return false;
}

void ChartWorld::buildDynamicStandeesAndTelegraphs() {
    if (activeStandees_.size() < staticStandees_.size()) {
        activeStandees_ = staticStandees_;
    } else {
        activeStandees_.resize(staticStandees_.size());
    }
    activeTelegraphs_.clear();

    // 1. Player Arcane Spell Telegraph Ring (Cat Quest Astropaw / Flamepurr nova)
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

    // 2. Hero Standee (Armored Cat Adventurer on land with hopping animation, Inked Caravel Ship at sea!)
    StandeeInstance heroSt{};
    heroSt.worldPos[0] = hero_.pos.x;
    heroSt.worldPos[2] = hero_.pos.z;

    if (hero_.isSailing) {
        // Authentically grounded in water: draft offset seats the bottom of the hull below the water plane,
        // so the 3D water surface naturally intersects the hull timbers without exposing the keel.
        heroSt.worldPos[1] = hero_.pos.y - 0.38f + std::sin(simTimeSec_ * 1.6f) * 0.05f;
        heroSt.width       = 3.2f;
        heroSt.height      = 3.2f;
        heroSt.texLayer    = InkLayer::StandeeShipCaravel;
        heroSt.flags       = StandeeFlags::WindSway | StandeeFlags::PopUpTilt | (hero_.facingRight ? 0u : StandeeFlags::FlipX);
        heroSt.phaseOffset = simTimeSec_ * 1.5f;
    } else {
        // Cat Quest hopping bounce on land (tamed subtle step-hop)
        float hopY = hero_.hopArc * 0.18f;
        heroSt.worldPos[1] = hero_.pos.y + hopY;

        // Dynamic subtle squash-and-stretch
        float stretch = (hero_.hopArc - 0.25f) * 0.06f;
        heroSt.width       = 2.2f * (1.0f - stretch);
        heroSt.height      = 2.4f * (1.0f + stretch);
        heroSt.texLayer    = InkLayer::StandeeHeroCat;
        heroSt.flags       = StandeeFlags::PopUpTilt | (hero_.facingRight ? 0u : StandeeFlags::FlipX);
        heroSt.phaseOffset = simTimeSec_ * 3.0f;
    }
    activeStandees_.push_back(heroSt);

    // 3. Floating Companion Spirit Standee
    StandeeInstance compSt{};
    compSt.worldPos[0] = hero_.pos.x - (hero_.facingRight ? 1.5f : -1.5f);
    compSt.worldPos[1] = hero_.pos.y + (hero_.isSailing ? 1.4f : 0.8f) + std::sin(simTimeSec_ * 3.2f) * 0.22f;
    compSt.worldPos[2] = hero_.pos.z + 0.6f;
    compSt.width       = 1.3f;
    compSt.height      = 1.3f;
    compSt.texLayer    = InkLayer::StandeeHeroCompanion;
    compSt.flags       = StandeeFlags::Emissive | StandeeFlags::IdleBob;
    compSt.phaseOffset = simTimeSec_;
    activeStandees_.push_back(compSt);

    // 4. Active Melee Slash Crescent
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

    // 5. Live Enemies & their Ground-Projected Combat Telegraphs (prioritized before ambient beacons)
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

        if (en.telegraphProgress > 0.01f && activeTelegraphs_.size() < 64) {
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

    // 6. Quest / Portal Beacon ring on the parchment around nearby Dungeon Portals (distance-culled)
    constexpr float kMaxBeaconDistSq = 95.0f * 95.0f;
    float beaconProgress = 0.5f + 0.5f * std::sin(simTimeSec_ * 2.5f);
    for (const auto& p : voronoi_.pois()) {
        if (activeTelegraphs_.size() >= 64) break;
        if (p.category == PoiCategory::DungeonPortal || p.category == PoiCategory::BossLair) {
            float dx = p.pos.x - hero_.pos.x;
            float dz = p.pos.y - hero_.pos.z;
            if (dx * dx + dz * dz > kMaxBeaconDistSq) continue;

            CombatTelegraph q{};
            q.center[0] = p.pos.x;
            q.center[1] = p.elevation;
            q.center[2] = p.pos.y;
            q.radius    = 2.8f;
            q.progress  = beaconProgress;
            q.telegraphType = TelegraphType::QuestBeacon;
            activeTelegraphs_.push_back(q);
        }
    }

    // 7. Active Point Lights: Hero Torchlight prioritized first, followed by nearby Chunk + Custom lights
    pointLights_.clear();
    pointLights_.reserve(64);

    PointLight heroTorch{};
    heroTorch.position[0] = hero_.pos.x;
    heroTorch.position[1] = hero_.pos.y + 1.25f;
    heroTorch.position[2] = hero_.pos.z;
    heroTorch.radius      = 16.0f;
    heroTorch.color[0]    = 1.0f;
    heroTorch.color[1]    = 0.74f;
    heroTorch.color[2]    = 0.38f;
    float flicker = 1.0f + 0.07f * std::sin(simTimeSec_ * 7.5f) + 0.04f * std::sin(simTimeSec_ * 13.1f);
    heroTorch.intensity   = 1.45f * flicker;
    pointLights_.push_back(heroTorch);

    for (const auto& pl : customPointLights_) {
        if (pointLights_.size() >= 64) break;
        pointLights_.push_back(pl);
    }
    for (const auto& pl : staticChunkLights_) {
        if (pointLights_.size() >= 64) break;
        pointLights_.push_back(pl);
    }
}

void ChartWorld::setWeatherType(WeatherType type, float intensity) noexcept {
    weather_.type = type;
    weather_.intensity = std::clamp(intensity, 0.0f, 1.0f);
    weather_.autoCycle = false; // Manual lock override
    if (type == WeatherType::Clear) {
        weather_.lightningFlash = 0.0f;
    }
}

ChartWorld::LocalWeatherInfo ChartWorld::evaluateLocalWeather(float wx, float wz) const noexcept {
    LocalWeatherInfo info{};
    info.type = weather_.type;
    info.intensity = weather_.intensity;

    if (voronoi_.isDungeonMode()) {
        info.temperature = 14.0f; // Subterranean constant cool vault
        info.precipitation = 0.0f;
        info.snowFactor = 0.0f;
        info.wetness = 0.0f;
        info.intensity = 0.0f;
        info.description = "Subterranean Vault";
        return info;
    }

    ChartFieldSample s = voronoi_.sampleAt(wx, wz);
    BiomeClimateProfile profile = getBiomeClimateProfile(s.biomePrimary);

    // Continental province thermal baseline
    float baseTemp = profile.baseTemp;
    const float dHighlands = (wx * wx + (wz - 190.0f) * (wz - 190.0f));
    const float dDesert    = ((wx - 190.0f) * (wx - 190.0f) + wz * wz);
    if (dHighlands < 150.0f * 150.0f) {
        float tH = 1.0f - std::sqrt(dHighlands) / 150.0f;
        baseTemp = (1.0f - tH) * baseTemp + tH * 2.0f; // Cool Silvervale highlands
    } else if (dDesert < 150.0f * 150.0f) {
        float tD = 1.0f - std::sqrt(dDesert) / 150.0f;
        baseTemp = (1.0f - tD) * baseTemp + tD * 34.0f; // Hot Sunscorched Expanse
    }

    // Atmospheric lapse rate: ~1.65°C drop per meter of tabletop elevation
    info.temperature = baseTemp - s.elevation * 1.65f;

    // Precipitation scaling based on global weather type and biome humidity
    float globalPrecip = 0.0f;
    switch (weather_.type) {
        case WeatherType::Drizzle:      globalPrecip = 0.35f; break;
        case WeatherType::Downpour:     globalPrecip = 0.80f; break;
        case WeatherType::Thunderstorm: globalPrecip = 1.00f; break;
        case WeatherType::Flurries:     globalPrecip = 0.35f; break;
        case WeatherType::Blizzard:     globalPrecip = 1.00f; break;
        case WeatherType::Sandstorm:    globalPrecip = 0.70f; break;
        default:                        globalPrecip = 0.0f; break;
    }
    info.precipitation = std::clamp(globalPrecip * weather_.intensity * profile.humidityMult, 0.0f, 1.0f);

    // Thermodynamic state classification:
    // If cold enough (T <= 0°C), rain converts to snow!
    if (info.temperature <= 0.0f) {
        if (info.precipitation > 0.02f) {
            if (info.precipitation > 0.60f) {
                info.type = WeatherType::Blizzard;
                info.snowFactor = std::min(1.0f, info.precipitation * 1.25f);
                info.description = "Alpine Mountain Blizzard";
            } else {
                info.type = WeatherType::Flurries;
                info.snowFactor = std::min(1.0f, info.precipitation * 1.10f);
                info.description = (weather_.intensity <= 0.25f) ? "Delicate Light Snow" : "Highland Snow Flurries";
            }
        } else {
            info.snowFactor = std::clamp((-info.temperature) / 8.0f, 0.0f, 0.6f);
            info.description = (s.elevation >= profile.minSnowAltitude) ? "Frosty Snow Chill" : "Freezing Mist";
        }
    } else if (profile.baseTemp > 26.0f && profile.humidityMult < 0.25f && info.precipitation > 0.20f) {
        // In arid desert, storms create sandstorms/haboobs
        info.type = WeatherType::Sandstorm;
        info.snowFactor = 0.0f;
        info.description = "Sunscorched Sandstorm";
    } else {
        info.snowFactor = 0.0f;
        info.description = weatherTypeName(info.type);
    }

    info.wetness = weather_.wetness;
    return info;
}

void ChartWorld::updateSimulation(float dt, uint32_t viewportW, uint32_t viewportH) {
    servicePendingChunkBakes();
    simTimeSec_ += dt;
    if (useDayNightCycle_) {
        atmosphere_.timeOfDay = std::fmod(atmosphere_.timeOfDay + dt * dayCycleSpeed_, 1.0f);
        if (atmosphere_.timeOfDay < 0.0f) atmosphere_.timeOfDay += 1.0f;
    }

    // Procedural Weather Progression & Environmental Dynamics
    if (weather_.autoCycle && !voronoi_.isDungeonMode()) {
        weatherCycleTimer_ += dt * weather_.cycleSpeed;
        if (weatherCycleTimer_ >= 1.0f) weatherCycleTimer_ -= 1.0f;

        ChartFieldSample heroSample = voronoi_.sampleAt(hero_.pos.x, hero_.pos.z);
        BiomeClimateProfile profile = getBiomeClimateProfile(heroSample.biomePrimary);

        float tVal = weatherCycleTimer_;
        float wSum = 0.0f;
        for (float w : profile.weatherWeights) wSum += w;

        WeatherType chosenType = profile.defaultWeather;
        float chosenIntensity = 0.0f;

        if (wSum > 1e-4f) {
            float normT = tVal * wSum;
            float acc = 0.0f;
            for (size_t wi = 0; wi < profile.weatherWeights.size(); ++wi) {
                float nextAcc = acc + profile.weatherWeights[wi];
                if (normT >= acc && normT < nextAcc) {
                    chosenType = static_cast<WeatherType>(wi);
                    float span = std::max(0.01f, profile.weatherWeights[wi]);
                    float localP = (normT - acc) / span;
                    chosenIntensity = std::sin(localP * 3.14159265f);
                    break;
                }
                acc = nextAcc;
            }
        }

        weather_.type = chosenType;
        if (chosenType == WeatherType::Clear) {
            weather_.intensity = 0.0f;
        } else if (chosenType == WeatherType::Flurries) {
            weather_.intensity = std::clamp(chosenIntensity * 0.40f + 0.15f, 0.15f, 0.60f);
        } else if (chosenType == WeatherType::Drizzle) {
            weather_.intensity = std::clamp(chosenIntensity * 0.35f + 0.15f, 0.15f, 0.55f);
        } else {
            weather_.intensity = std::clamp(chosenIntensity * 0.65f + 0.35f, 0.35f, 1.0f);
        }
    }

    // Evaluate local climate conditions around the hero
    LocalWeatherInfo heroWeather = evaluateLocalWeather(hero_.pos.x, hero_.pos.z);

    // Dynamic ground wetness accumulation & drying (liquid rain accumulates puddles, snow does not)
    if (heroWeather.precipitation > 0.05f && heroWeather.temperature > 0.0f) {
        weather_.wetness = std::min(1.0f, weather_.wetness + dt * 0.14f * heroWeather.precipitation);
    } else {
        weather_.wetness = std::max(0.0f, weather_.wetness - dt * 0.038f);
    }

    // Dynamic snowline descent & accumulation
    if (heroWeather.snowFactor > 0.05f) {
        weather_.snowCover = std::min(1.0f, weather_.snowCover + dt * 0.18f * heroWeather.snowFactor);
    } else {
        weather_.snowCover = std::max(0.0f, weather_.snowCover - dt * 0.045f);
    }

    // Ocean storm surge / wave chop
    if (weather_.type == WeatherType::Thunderstorm) {
        weather_.stormSurge = std::min(1.0f, weather_.stormSurge + dt * 0.30f);
    } else {
        weather_.stormSurge = std::max(0.0f, weather_.stormSurge - dt * 0.12f);
    }

    // Lightning Flash Scheduler during Thunderstorms
    if (weather_.type == WeatherType::Thunderstorm && !voronoi_.isDungeonMode()) {
        lightningTimer_ -= dt;
        if (lightningTimer_ <= 0.0f) {
            weather_.lightningFlash = 1.0f;
            uint32_t seedFlash = hashU32(static_cast<uint32_t>(simTimeSec_ * 100.0f));
            lightningTimer_ = 3.2f + static_cast<float>(seedFlash % 100) * 0.045f; // 3.2s to 7.7s
            if (soundCb_) {
                soundCb_(FoleySound::ThunderClap, 0.95f);
            }
        }
    }
    if (weather_.lightningFlash > 0.0f) {
        weather_.lightningFlash = std::max(0.0f, weather_.lightningFlash - dt * 7.5f);
    }

    if (hero_.attackTimer > 0.0f) hero_.attackTimer = std::max(0.0f, hero_.attackTimer - dt);
    if (hero_.spellTimer > 0.0f)  hero_.spellTimer  = std::max(0.0f, hero_.spellTimer - dt);

    // Advance Diegetic Fog-of-War & Ink Bloom discovery around Hero
    int heroCx = static_cast<int>(std::floor(hero_.pos.x / kChunkSize));
    int heroCz = static_cast<int>(std::floor(hero_.pos.z / kChunkSize));
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            int cx = heroCx + dx;
            int cz = heroCz + dz;
            uint64_t key = packChunkKey(cx, cz);
            auto it = chunkCache_.find(key);
            if (it != chunkCache_.end()) {
                float distSq = static_cast<float>(dx * dx + dz * dz);
                float rate = (distSq <= 1.0f) ? 3.2f : (distSq <= 2.0f ? 1.8f : 0.9f);
                it->second.discovery = std::min(1.0f, it->second.discovery + dt * rate);
            }
        }
    }

    // Prune dead enemies and far-off non-boss wilderness enemies so the 24-enemy cap never locks out new spawns
    const bool inDungeon = voronoi_.isDungeonMode();
    enemies_.erase(
        std::remove_if(enemies_.begin(), enemies_.end(), [&](const EnemyAgent& en) {
            if (!en.alive) return true;
            if (!inDungeon && !en.isBoss) {
                float dx = hero_.pos.x - en.pos.x;
                float dz = hero_.pos.z - en.pos.z;
                if (dx * dx + dz * dz > 160.0f * 160.0f) return true;
            }
            return false;
        }),
        enemies_.end()
    );

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

    // Advance ephemeral locale discovery banner timer
    if (localeBanner_.active) {
        localeBanner_.timer += dt;
        if (localeBanner_.timer >= localeBanner_.duration) {
            localeBanner_.active = false;
        }
    }

    buildDynamicStandeesAndTelegraphs();
    buildHudAndMapOverlay(viewportW, viewportH);
}

void ChartWorld::buildHudAndMapOverlay(uint32_t viewportW, uint32_t viewportH) {
    float vpW = static_cast<float>(std::max(1u, viewportW));
    float vpH = static_cast<float>(std::max(1u, viewportH));

    overlay2D_.beginFrame(vpW, vpH);

#if !defined(__EMSCRIPTEN__)
    // Top Cartographic Parchment Ribbon Banner (Native Desktop Window; Web uses HTML/CSS Top Bar & Status Pill)
    float barW = std::min(780.0f, vpW - 32.0f);
    float barX = (vpW - barW) * 0.5f;
    float barY = 12.0f;
    float barH = 44.0f;

    overlay2D_.drawRoundedRect(barX, barY, barW, barH, 6.0f, 0.14f, 0.11f, 0.08f, 0.86f);
    overlay2D_.drawRoundedRectOutline(barX, barY, barW, barH, 6.0f, 0.82f, 0.68f, 0.42f, 0.92f, 1.5f);

    // Left: Active Region / POI Title & Cartographic Time of Day Watch
    char watchBuf[128];
    int hour = static_cast<int>(atmosphere_.timeOfDay * 24.0f) % 24;
    int minute = static_cast<int>(std::fmod(atmosphere_.timeOfDay * 1440.0f, 60.0f));
    LocalWeatherInfo heroWtr = evaluateLocalWeather(hero_.pos.x, hero_.pos.z);
    std::snprintf(watchBuf, sizeof(watchBuf), " - %02d:%02d [%s | %s]", hour, minute, AtmosphereConfig::timePeriodName(atmosphere_.timeOfDay), heroWtr.description);

    std::string regionHeader = hero_.activeRegionName + (hero_.isSailing ? " [SAILING]" : "");
    std::string subTitle = hero_.activeRegionSub + watchBuf;
    eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + 16.0f, barY + 8.0f, 15.0f, regionHeader, 0.96f, 0.88f, 0.68f, 1.0f);
    eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + 16.0f, barY + 26.0f, 11.0f, subTitle, 0.78f, 0.72f, 0.58f, 0.95f);

    // Right: Cat Quest Hero Stats (Lv, HP, Gold, Controls)
    char statBuf[128];
    std::snprintf(statBuf, sizeof(statBuf), "LV.%d  HP %d/%d  GOLD %d  CHARTED:%u", hero_.level, hero_.hp, hero_.maxHp, hero_.gold, discoveredChunkCount());
    float statW = eatsbits::ui::glyph::getVectorTextWidth(13.0f, statBuf);
    eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + barW - statW - 16.0f, barY + 9.0f, 13.0f, statBuf, 0.98f, 0.82f, 0.38f, 1.0f);

    if (barW >= 620.0f) {
        const char* ctrlHint = "WASD:Move  SPACE:Slash  K:Spell  T:Day/Night  Y:Weather  [/]:Time  E:Dungeon  M:Atlas  1-6:Charts";
        float hintW = eatsbits::ui::glyph::getVectorTextWidth(10.0f, ctrlHint);
        eatsbits::ui::glyph::drawVectorText(overlay2D_, barX + barW - hintW - 16.0f, barY + 27.0f, 10.0f, ctrlHint, 0.76f, 0.74f, 0.68f, 0.9f);
    }
#endif

    // Ephemeral Region Discovery Banner (Shared C++ Engine Implementation: Native Desktop & Web)
    if (localeBanner_.active) {
        float t = localeBanner_.timer;
        float alpha = 0.0f;
        float yOffset = 0.0f;
        constexpr float kFadeIn = 0.40f;
        constexpr float kFadeOut = 0.60f;
        float sustainEnd = std::max(kFadeIn, localeBanner_.duration - kFadeOut);

        if (t < kFadeIn) {
            float p = t / kFadeIn;
            alpha = p * p * (3.0f - 2.0f * p);
            yOffset = (1.0f - p) * -8.0f;
        } else if (t < sustainEnd) {
            alpha = 1.0f;
            yOffset = 0.0f;
        } else if (t < localeBanner_.duration) {
            float p = (t - sustainEnd) / kFadeOut;
            alpha = 1.0f - (p * p * (3.0f - 2.0f * p));
            yOffset = p * 4.0f;
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        if (alpha > 0.005f && !localeBanner_.title.empty()) {
            float titleFontSize = 16.0f;
            float subFontSize = 10.5f;
            float titleW = eatsbits::ui::glyph::getVectorTextWidth(titleFontSize, localeBanner_.title);
            float subW = localeBanner_.subtitle.empty() ? 0.0f : eatsbits::ui::glyph::getVectorTextWidth(subFontSize, localeBanner_.subtitle);

            float contentW = std::max(titleW + 68.0f, subW + 40.0f);
            float maxPillW = std::min(560.0f, vpW - 32.0f);
            float pillW = std::clamp(contentW + 28.0f, 220.0f, maxPillW);
            float pillH = localeBanner_.subtitle.empty() ? 38.0f : 52.0f;

            float pillX = (vpW - pillW) * 0.5f;
            float baseY = 64.0f;
            float pillY = baseY + yOffset;

            // Outer subtle drop-shadow
            overlay2D_.drawRoundedRect(pillX - 2.0f, pillY + 2.0f, pillW + 4.0f, pillH + 4.0f, 14.0f, 0.02f, 0.015f, 0.01f, 0.42f * alpha);

            // Dark vellum/obsidian card background
            overlay2D_.drawRoundedRect(pillX, pillY, pillW, pillH, 12.0f, 0.10f, 0.075f, 0.05f, 0.88f * alpha);

            // Warm gilded brass border outline
            overlay2D_.drawRoundedRectOutline(pillX, pillY, pillW, pillH, 12.0f, 0.85f, 0.70f, 0.35f, 0.90f * alpha, 1.5f);

            // Top rim gilded highlight
            overlay2D_.drawLine(pillX + 16.0f, pillY + 2.0f, pillX + pillW - 16.0f, pillY + 2.0f, 0.98f, 0.86f, 0.48f, 0.32f * alpha, 1.0f);

            float titleX = pillX + (pillW - titleW) * 0.5f;
            float titleY = pillY + (localeBanner_.subtitle.empty() ? 11.0f : 9.0f);

            // Flanking Diamond Flourishes (✦)
            float diamondLeftX = titleX - 16.0f;
            float diamondRightX = titleX + titleW + 16.0f;
            float diamondY = titleY + titleFontSize * 0.5f - 1.0f;
            float dW = 4.0f;
            float dH = 5.5f;

            auto drawDiamond = [&](float cx, float cy, float szW, float szH, float r, float g, float b, float a) {
                overlay2D_.drawTriangle(cx, cy - szH, cx + szW, cy, cx, cy + szH, r, g, b, a);
                overlay2D_.drawTriangle(cx, cy - szH, cx, cy + szH, cx - szW, cy, r, g, b, a);
            };

            // Diamond shadow
            drawDiamond(diamondLeftX + 1.0f, diamondY + 1.0f, dW, dH, 0.04f, 0.03f, 0.02f, 0.75f * alpha);
            drawDiamond(diamondRightX + 1.0f, diamondY + 1.0f, dW, dH, 0.04f, 0.03f, 0.02f, 0.75f * alpha);
            // Diamond face
            drawDiamond(diamondLeftX, diamondY, dW, dH, 0.96f, 0.78f, 0.28f, 0.95f * alpha);
            drawDiamond(diamondRightX, diamondY, dW, dH, 0.96f, 0.78f, 0.28f, 0.95f * alpha);

            // Flanking horizontal wing accent lines
            overlay2D_.drawLine(diamondLeftX - 18.0f, diamondY, diamondLeftX - 6.0f, diamondY, 0.85f, 0.70f, 0.35f, 0.45f * alpha, 1.0f);
            overlay2D_.drawLine(diamondRightX + 6.0f, diamondY, diamondRightX + 18.0f, diamondY, 0.85f, 0.70f, 0.35f, 0.45f * alpha, 1.0f);

            // Title drop-shadow + text
            eatsbits::ui::glyph::drawVectorText(overlay2D_, titleX + 1.5f, titleY + 1.5f, titleFontSize, localeBanner_.title, 0.04f, 0.03f, 0.02f, 0.85f * alpha);
            eatsbits::ui::glyph::drawVectorText(overlay2D_, titleX, titleY, titleFontSize, localeBanner_.title, 0.99f, 0.95f, 0.84f, alpha);

            // Subtitle & separator if present
            if (!localeBanner_.subtitle.empty()) {
                float divY = titleY + titleFontSize + 4.0f;
                float divHalfW = std::min(pillW * 0.38f, (subW + 36.0f) * 0.5f);
                float centerX = vpW * 0.5f;
                overlay2D_.drawLine(centerX - divHalfW, divY, centerX + divHalfW, divY, 0.82f, 0.68f, 0.38f, 0.32f * alpha, 1.0f);
                drawDiamond(centerX, divY, 2.5f, 2.5f, 0.95f, 0.80f, 0.40f, 0.75f * alpha);

                float subX = pillX + (pillW - subW) * 0.5f;
                float subY = divY + 4.0f;
                eatsbits::ui::glyph::drawVectorText(overlay2D_, subX + 1.0f, subY + 1.0f, subFontSize, localeBanner_.subtitle, 0.04f, 0.03f, 0.02f, 0.75f * alpha);
                eatsbits::ui::glyph::drawVectorText(overlay2D_, subX, subY, subFontSize, localeBanner_.subtitle, 0.94f, 0.82f, 0.48f, 0.92f * alpha);
            }
        }
    }

    // Project 3D Floating Combat / Level-Up Numbers into 2D Screen Space
    if (!floatingNumbers_.empty()) {
        Vec3f f{camera_.target.x - camera_.position.x, camera_.target.y - camera_.position.y, camera_.target.z - camera_.position.z};
        float fLen = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
        if (fLen > 1e-6f) {
            f.x /= fLen; f.y /= fLen; f.z /= fLen;
            Vec3f up{0.0f, 1.0f, 0.0f};
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

            float aspect = vpW / vpH;
            float fovRad = camera_.fovDegrees * (3.1415926535f / 180.0f);
            float projScale = 1.0f / std::tan(fovRad * 0.5f);
            float effCurve = camera_.horizonCurve * (1.0f - camera_.atlasZoom * 0.85f);

            for (const auto& fn : floatingNumbers_) {
                float dz = std::max(0.0f, (fn.worldPos.z - hero_.pos.z) - 8.0f);
                float dx = (fn.worldPos.x - hero_.pos.x) * 0.35f;
                float curveDrop = (dz * dz + dx * dx) * effCurve;
                Vec3f wp{fn.worldPos.x, fn.worldPos.y - curveDrop, fn.worldPos.z};

                Vec3f rel{wp.x - camera_.position.x, wp.y - camera_.position.y, wp.z - camera_.position.z};
                float vx = rel.x * s.x + rel.y * s.y + rel.z * s.z;
                float vy = rel.x * u.x + rel.y * u.y + rel.z * u.z;
                float vz = -(rel.x * f.x + rel.y * f.y + rel.z * f.z);
                if (vz < -1.0f && vz > -260.0f) {
                    float clipW = -vz;
                    float ndcX = (vx * (projScale / aspect)) / clipW;
                    float ndcY = (vy * projScale) / clipW;
                    if (ndcX >= -1.1f && ndcX <= 1.1f && ndcY >= -1.1f && ndcY <= 1.1f) {
                        float sx = (ndcX * 0.5f + 0.5f) * vpW;
                        float sy = (0.5f - ndcY * 0.5f) * vpH;
                        float alpha = std::clamp(1.0f - (fn.age / std::max(0.1f, fn.lifetime)), 0.0f, 1.0f);
                        float fontSize = 18.0f;
                        float tw = eatsbits::ui::glyph::getVectorTextWidth(fontSize, fn.text);
                        eatsbits::ui::glyph::drawVectorText(overlay2D_, sx - tw * 0.5f + 1.5f, sy + 1.5f, fontSize, fn.text,
                            0.06f, 0.04f, 0.02f, alpha * 0.9f);
                        eatsbits::ui::glyph::drawVectorText(overlay2D_, sx - tw * 0.5f, sy, fontSize, fn.text,
                            fn.color.r / 255.0f, fn.color.g / 255.0f, fn.color.b / 255.0f, alpha);
                    }
                }
            }
        }
    }
}

RaycastHit ChartWorld::raycast(Vec3f origin, Vec3f dir, float maxDist) const noexcept {
    RaycastHit result{};
    float step = 0.75f;
    for (float t = 1.0f; t <= maxDist; t += step) {
        Vec3f p{origin.x + dir.x * t, origin.y + dir.y * t, origin.z + dir.z * t};
        if (std::abs(p.x - gridCenter_.x) <= kChunkRadius * kChunkSize && std::abs(p.z - gridCenter_.y) <= kChunkRadius * kChunkSize) {
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
