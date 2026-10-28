#include "eatscharted/voronoi_atlas.hpp"
#include "eatscharted/procedural_names.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

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

[[nodiscard]] inline uint64_t packBlockCoord(int bx, int bz) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(bx)) << 32u) |
           static_cast<uint64_t>(static_cast<uint32_t>(bz));
}

[[nodiscard]] float hashFloat(int x, int z, uint32_t seed) noexcept {
    uint32_t h = hashU32(static_cast<uint32_t>(x) * 374761393u +
                         static_cast<uint32_t>(z) * 668265263u +
                         seed * 1442695041u);
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

[[nodiscard]] float smoothValueNoise(float x, float z, uint32_t seed) noexcept {
    int ix = static_cast<int>(std::floor(x));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - static_cast<float>(ix);
    float fz = z - static_cast<float>(iz);

    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);

    float n00 = hashFloat(ix,     iz,     seed);
    float n10 = hashFloat(ix + 1, iz,     seed);
    float n01 = hashFloat(ix,     iz + 1, seed);
    float n11 = hashFloat(ix + 1, iz + 1, seed);

    float nx0 = n00 + u * (n10 - n00);
    float nx1 = n01 + u * (n11 - n01);
    return (nx0 + v * (nx1 - nx0)) * 2.0f - 1.0f;
}

[[nodiscard]] float fbmNoise(float x, float z, uint32_t seed, int octaves = 5) noexcept {
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * smoothValueNoise(x * freq, z * freq, seed + static_cast<uint32_t>(i) * 101u);
        freq *= 2.07f;
        amp *= 0.5f;
    }
    return sum;
}

[[nodiscard]] float pointToSegmentDist(Vec2f p, Vec2f a, Vec2f b, float& outT) noexcept {
    float vx = b.x - a.x;
    float vy = b.y - a.y;
    float wx = p.x - a.x;
    float wy = p.y - a.y;
    float lenSq = vx * vx + vy * vy;
    float t = (lenSq > 1e-6f) ? ((wx * vx + wy * vy) / lenSq) : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    outT = t;
    float dx = p.x - (a.x + t * vx);
    float dy = p.y - (a.y + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] BiomeId classifyBiome(float elevMeters, float moisture, float coastDist, uint32_t seedHash) noexcept {
    if (elevMeters <= -2.2f) return BiomeId::OceanDeep;
    if (elevMeters <= 0.0f)  return BiomeId::CoastalShoals;
    if (coastDist < 3.4f && elevMeters < 1.35f) {
        return (moisture > 0.66f) ? BiomeId::SaltMarsh : BiomeId::SandyBeach;
    }
    if (elevMeters > 7.2f) {
        return BiomeId::AlpinePeak;
    }
    if (elevMeters > 4.2f) {
        return (moisture > 0.42f) ? BiomeId::TaigaPines : BiomeId::BadlandsCanyon;
    }
    if ((seedHash & 31u) == 0u && elevMeters > 2.2f && moisture < 0.35f) {
        return BiomeId::CalamityBlight;
    }
    if (moisture > 0.68f) return BiomeId::DeciduousWoodland;
    if (moisture > 0.36f) return BiomeId::LushMeadow;
    if (moisture > 0.18f) return BiomeId::AridSteppe;
    return BiomeId::BadlandsCanyon;
}

} // namespace

void VoronoiAtlas::generate(uint32_t seed, AtlasPreset preset) {
    seed_ = seed;
    preset_ = preset;
    cells_.clear();
    rivers_.clear();
    roads_.clear();
    pois_.clear();
    banners_.clear();
    dungeonRooms_.clear();
    generatedBlocks_.clear();
    generatedBlocks_.push_back(packBlockCoord(0, 0));

    if (preset_ == AtlasPreset::OnePageDungeon) {
        generateOnePageDungeonLayout();
    } else {
        generateOverworldGraph();
    }
}

float VoronoiAtlas::rawElevationAt(float wx, float wz) const noexcept {
    // Domain-warped fractal perturbation for authentic Watabou Perilous Shores jagged coastlines
    float warpX = fbmNoise(wx * 0.018f, wz * 0.018f, seed_ + 17u, 3) * 14.0f;
    float warpZ = fbmNoise(wx * 0.018f + 9.3f, wz * 0.018f - 4.7f, seed_ + 31u, 3) * 14.0f;
    float qx = wx + warpX;
    float qz = wz + warpZ;

    float detail = fbmNoise(qx * 0.032f, qz * 0.032f, seed_ + 53u, 5);
    float ridge  = 1.0f - std::abs(fbmNoise(qx * 0.022f, qz * 0.022f, seed_ + 79u, 4));
    ridge = ridge * ridge;

    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // Sweeping mainland peninsula entering from the North/East with a sheltered bay in the South-West
        // and offshore barrier islands & sea stacks
        float mainlandBias = (qz * 0.014f) + (qx * 0.008f) + 0.22f;
        // Carve a dramatic sheltered harbor bay around (-16, -12)
        float bayDx = (qx + 18.0f) / 34.0f;
        float bayDz = (qz + 14.0f) / 28.0f;
        float bayCarve = std::exp(-(bayDx * bayDx + bayDz * bayDz) * 1.4f) * 0.72f;

        // Offshore island chain in the South-West (-42, -36) & (26, -38)
        float isl1Dx = (qx + 38.0f) / 18.0f;
        float isl1Dz = (qz + 32.0f) / 16.0f;
        float isl1 = std::exp(-(isl1Dx * isl1Dx + isl1Dz * isl1Dz) * 1.6f) * 0.68f;

        float isl2Dx = (qx - 28.0f) / 16.0f;
        float isl2Dz = (qz + 34.0f) / 14.0f;
        float isl2 = std::exp(-(isl2Dx * isl2Dx + isl2Dz * isl2Dz) * 1.8f) * 0.62f;

        // Northern alpine ridge spine
        float spineDz = (qz - 42.0f) / 24.0f;
        float spine = std::exp(-(spineDz * spineDz)) * ridge * 1.25f;

        // Sorcery! tactile rolling mounds & foothills
        float mounds = std::pow(std::max(0.0f, fbmNoise(qx * 0.048f + 5.2f, qz * 0.048f - 3.1f, seed_ + 89u, 3)), 1.8f) * 0.72f;
        float crags = (1.0f - std::abs(fbmNoise(qx * 0.055f, qz * 0.055f, seed_ + 103u, 3))) * ridge * 0.55f;

        float hNorm = mainlandBias - bayCarve + isl1 + isl2 + spine + mounds + crags + detail * 0.42f;
        return hNorm * 6.5f;
    } else if (preset_ == AtlasPreset::ShatteredArchipelago) {
        // Multi-island nautical chart with central lagoon & surrounding coral/rocky islands
        float r = std::sqrt(qx * qx + qz * qz);
        float ring = std::exp(-std::pow((r - 36.0f) / 22.0f, 2.0f)) * 0.55f;
        float centerIsle = std::exp(-(qx * qx + qz * qz) / (18.0f * 18.0f)) * 0.75f;
        float mounds = std::pow(std::max(0.0f, fbmNoise(qx * 0.05f, qz * 0.05f, seed_ + 89u, 3)), 1.6f) * 0.55f;
        float archipelago = fbmNoise(qx * 0.028f, qz * 0.028f, seed_ + 113u, 5) * 0.85f;
        float hNorm = ring + centerIsle + archipelago + mounds - 0.18f;
        return hNorm * 6.2f;
    } else {
        // HighlandKingdom: Twin mountain ranges flanking a lush central river valley
        float valleyDist = std::abs(qx - std::sin(qz * 0.035f) * 18.0f);
        float valleyProfile = std::clamp((valleyDist - 10.0f) / 42.0f, -0.25f, 1.1f);
        float southernCoast = std::clamp((qz + 58.0f) / 25.0f, -0.6f, 0.4f);
        float crags = (1.0f - std::abs(fbmNoise(qx * 0.045f, qz * 0.045f, seed_ + 117u, 3))) * ridge * 0.65f;
        float mounds = std::pow(std::max(0.0f, fbmNoise(qx * 0.042f, qz * 0.042f, seed_ + 71u, 3)), 1.7f) * 0.60f;
        float hNorm = valleyProfile * 0.85f + southernCoast * 0.45f + detail * 0.42f + crags + mounds;
        return hNorm * 7.2f;
    }
}

float VoronoiAtlas::rawMoistureAt(float wx, float wz) const noexcept {
    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // High moisture in northern/western peninsula interior (Duskwoods), arid in central/southern plains (Dragon Desert)
        float forestBias = std::exp(-((wx + 8.0f) * (wx + 8.0f) + (wz - 34.0f) * (wz - 34.0f)) / (22.0f * 22.0f)) * 0.78f;
        float desertBias = std::exp(-((wx - 14.0f) * (wx - 14.0f) + (wz + 6.0f) * (wz + 6.0f)) / (26.0f * 26.0f)) * 0.42f;
        float m = 0.42f + forestBias - desertBias + fbmNoise(wx * 0.021f, wz * 0.021f, seed_ + 211u, 3) * 0.20f;
        return std::clamp(m, 0.0f, 1.0f);
    }
    float m = 0.52f + fbmNoise(wx * 0.021f + 31.0f, wz * 0.021f - 19.0f, seed_ + 211u, 4) * 0.48f;
    return std::clamp(m, 0.0f, 1.0f);
}

void VoronoiAtlas::addWindingRiver(Vec2f start, Vec2f mid, Vec2f mouth, float startWidth, float endWidth) {
    constexpr int kSteps = 18;
    Vec2f prev = start;
    for (int i = 1; i <= kSteps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSteps);
        float u = 1.0f - t;
        Vec2f pt{
            u * u * start.x + 2.0f * u * t * mid.x + t * t * mouth.x,
            u * u * start.y + 2.0f * u * t * mid.y + t * t * mouth.y
        };
        float meander = std::sin(t * 12.56f + static_cast<float>(seed_ % 7u)) * (1.0f - t * 0.5f) * 2.8f;
        pt.x += meander;
        float w0 = startWidth + (endWidth - startWidth) * (static_cast<float>(i - 1) / static_cast<float>(kSteps));
        float w1 = startWidth + (endWidth - startWidth) * t;
        rivers_.push_back(RiverSegment{prev, pt, w0, w1});
        prev = pt;
    }
}

void VoronoiAtlas::addCurvedRoad(Vec2f a, Vec2f ctrl, Vec2f b, bool isHighway) {
    constexpr int kSegs = 14;
    Vec2f prev = a;
    float accumArc = 0.0f;
    for (int i = 1; i <= kSegs; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSegs);
        float u = 1.0f - t;
        Vec2f pt{
            u * u * a.x + 2.0f * u * t * ctrl.x + t * t * b.x,
            u * u * a.y + 2.0f * u * t * ctrl.y + t * t * b.y
        };
        float segLen = std::sqrt((pt.x - prev.x) * (pt.x - prev.x) + (pt.y - prev.y) * (pt.y - prev.y));
        roads_.push_back(RoadSegment{prev, pt, accumArc, accumArc + segLen, isHighway});
        accumArc += segLen;
        prev = pt;
    }
}

void VoronoiAtlas::generateOverworldGraph() {
    // 1. Generate relaxed Voronoi sites across [-96m, +96m] domain
    constexpr int kGridN = 12;
    constexpr float kDomainMin = -96.0f;
    constexpr float kDomainMax =  96.0f;
    constexpr float kCellStep  = (kDomainMax - kDomainMin) / static_cast<float>(kGridN);

    cells_.reserve(kGridN * kGridN);
    int idCounter = 0;
    for (int gz = 0; gz < kGridN; ++gz) {
        for (int gx = 0; gx < kGridN; ++gx) {
            float jx = (hashFloat(gx, gz, seed_ + 1u) - 0.5f) * kCellStep * 0.72f;
            float jz = (hashFloat(gx, gz, seed_ + 2u) - 0.5f) * kCellStep * 0.72f;
            VoronoiRegionCell cell{};
            cell.id = idCounter++;
            cell.site = {
                kDomainMin + (static_cast<float>(gx) + 0.5f) * kCellStep + jx,
                kDomainMin + (static_cast<float>(gz) + 0.5f) * kCellStep + jz
            };
            cells_.push_back(cell);
        }
    }

    // Build approximate Delaunay adjacency (8-neighborhood in jittered grid + distance threshold)
    for (int gz = 0; gz < kGridN; ++gz) {
        for (int gx = 0; gx < kGridN; ++gx) {
            int idx = gz * kGridN + gx;
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dz == 0) continue;
                    int nx = gx + dx;
                    int nz = gz + dz;
                    if (nx >= 0 && nx < kGridN && nz >= 0 && nz < kGridN) {
                        cells_[idx].neighbors.push_back(nz * kGridN + nx);
                    }
                }
            }
        }
    }

    // Evaluate cell elevations, moisture, and downhill topology
    for (auto& c : cells_) {
        c.elevation = rawElevationAt(c.site.x, c.site.y);
        c.moisture  = rawMoistureAt(c.site.x, c.site.y);
        c.isWater   = (c.elevation <= 0.0f);
    }

    for (auto& c : cells_) {
        float minDistToFlip = 200.0f;
        for (const auto& other : cells_) {
            if (other.isWater != c.isWater) {
                float dx = c.site.x - other.site.x;
                float dy = c.site.y - other.site.y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < minDistToFlip) minDistToFlip = d;
            }
        }
        c.coastDist = c.isWater ? -minDistToFlip : minDistToFlip;
        c.biome = classifyBiome(c.elevation, c.moisture, std::abs(c.coastDist), hashU32(static_cast<uint32_t>(c.id) + seed_));

        float lowestElev = c.elevation;
        int lowestNbr = -1;
        for (int nIdx : c.neighbors) {
            if (cells_[nIdx].elevation < lowestElev) {
                lowestElev = cells_[nIdx].elevation;
                lowestNbr = nIdx;
            }
        }
        c.downhillNeighbor = lowestNbr;
    }

    // 2. Synthesize Watabou-style Winding Rivers from high hills down to the coast
    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // Meandering river starting in alpine mountains past Shattered Palace and Dragon Desert to the sea
        addWindingRiver({-12.0f, 6.0f}, {8.0f, -2.0f}, {32.0f, -8.0f}, 0.85f, 2.2f);
    } else if (preset_ == AtlasPreset::HighlandKingdom) {
        addWindingRiver({-4.0f, 70.0f}, {2.0f, 10.0f},   {-6.0f, -55.0f}, 1.2f, 3.2f);
        addWindingRiver({-44.0f, 38.0f}, {-20.0f, 14.0f},{-1.0f, 4.0f},   0.8f, 1.6f);
    } else {
        addWindingRiver({4.0f, 16.0f},  {0.0f, 4.0f},    {-10.0f, -12.0f},0.7f, 1.6f);
    }

    // 3. Place Cat Quest & Perilous Shores Landmarks / Settlements (POIs - Reference: Kildland Map)
    auto addPoi = [&](PoiCategory cat, float x, float z, const char* name, const char* sub, int lvl) {
        ChartPoi p{};
        p.category   = cat;
        p.pos        = {x, z};
        p.elevation  = std::max(0.4f, rawElevationAt(x, z));
        p.name       = name;
        p.subtitle   = sub;
        p.seed       = hashU32(static_cast<uint32_t>(pois_.size()) + seed_);
        p.questLevel = lvl;
        pois_.push_back(p);
    };

    if (preset_ == AtlasPreset::PerilousPeninsula) {
        std::string townName   = (seed_ == 1492u) ? "LOWMILLBURG" : ProceduralNameGenerator::generateLocaleName(seed_ + 1u, BiomeId::LushMeadow, PoiCategory::CitadelKeep);
        std::string hamletName = (seed_ == 1492u) ? "COLDHEARTFIELD" : ProceduralNameGenerator::generateLocaleName(seed_ + 2u, BiomeId::LushMeadow, PoiCategory::MarketHamlet);
        std::string portName   = (seed_ == 1492u) ? "STORMPOOL" : ProceduralNameGenerator::generateLocaleName(seed_ + 3u, BiomeId::CoastalShoals, PoiCategory::PortHarbor);
        std::string dungeon1   = (seed_ == 1492u) ? "SHATTERED PALACE" : ProceduralNameGenerator::generateLocaleName(seed_ + 4u, BiomeId::AlpinePeak, PoiCategory::DungeonPortal);
        std::string dungeon2   = (seed_ == 1492u) ? "FORT OF THE UNDEAD-ONE" : ProceduralNameGenerator::generateLocaleName(seed_ + 5u, BiomeId::CoastalShoals, PoiCategory::DungeonPortal);

        addPoi(PoiCategory::CitadelKeep,      6.0f, -34.0f, townName.c_str(),         "Southern Walled Citadel & Castle", 3);
        addPoi(PoiCategory::Windmill,         1.0f, -38.0f, "LOWMILL GRANGE",         "Farmland Grain Terraces",          1);
        addPoi(PoiCategory::Windmill,        11.0f, -38.0f, "WESTVALE MILL",          "Valley Windmill",                  1);
        addPoi(PoiCategory::MarketHamlet,    18.0f,  12.0f, hamletName.c_str(),       "Terracotta Hamlet & Fields",       2);
        addPoi(PoiCategory::PortHarbor,      32.0f, -12.0f, portName.c_str(),         "Eastern Nautical Cape & Harbor",   1);
        addPoi(PoiCategory::Lighthouse,      37.0f, -10.0f, "STORMPOOL BEACON",      "Watchtower of the Eastern Shoals", 2);
        addPoi(PoiCategory::DungeonPortal,  -14.0f,   4.0f, dungeon1.c_str(),         "Ancient Sunken Necropolis",        4);
        addPoi(PoiCategory::DungeonPortal,  -26.0f, -16.0f, dungeon2.c_str(),         "Coastal Fortress of Whispers",    5);
        addPoi(PoiCategory::AlpineMonastery, -2.0f,  46.0f, "STARFALL ABBEY",         "Highland Astrologer Spire",        5);
        addPoi(PoiCategory::SunkenRuin,      26.0f, -32.0f, "DROWNED OBELISK",        "Shattered Sea-King Ruins",         6);
    } else if (preset_ == AtlasPreset::ShatteredArchipelago) {
        addPoi(PoiCategory::PortHarbor,       0.0f,  -6.0f, "CORSAIR'S HAVEN",   "Central Atoll Anchorage",     2);
        addPoi(PoiCategory::Lighthouse,     -32.0f, -18.0f, "STORMGLASS TOWER",  "Western Reef Beacon",         3);
        addPoi(PoiCategory::DungeonPortal,   30.0f,  22.0f, "ABYSSAL GROTTO",    "Lv. 5 Flooded Vault",         5);
        addPoi(PoiCategory::SunkenRuin,     -24.0f,  28.0f, "SIREN'S ARCH",      "Coral-Draped Temple",         4);
        addPoi(PoiCategory::BossLair,        34.0f, -24.0f, "LEVIATHAN REEF",    "Lv. 10 Mauler of the Deep",  10);
    } else {
        addPoi(PoiCategory::CitadelKeep,      0.0f,   6.0f, "HIGHGUARD KEEP",    "Twin-Bridge Valley Fortress", 3);
        addPoi(PoiCategory::MarketHamlet,   -22.0f, -16.0f, "MEADOWBROOK",       "River Crossing Trading Post", 1);
        addPoi(PoiCategory::AlpineMonastery, 32.0f,  36.0f, "FROSTPEAK EYRIE",   "Cloud-Capped Sanctuary",      6);
        addPoi(PoiCategory::DungeonPortal,  -30.0f,  28.0f, "BLACKHOLLOW DELVE", "Lv. 5 Deep Mine Portal",      5);
    }

    // 4. Synthesize Spline Trade Roads & Dashed Wilderness Trails between POIs
    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // Reference Dotted Crimson Route: Coldheartfield -> Stormpool -> Lowmillburg
        addCurvedRoad({18.0f, 12.0f},  {26.0f, 0.0f},   {32.0f, -12.0f}, false);
        addCurvedRoad({32.0f, -12.0f}, {22.0f, -24.0f}, {6.0f, -34.0f},  false);
        // Wilderness trail to Shattered Palace and Fort Of The Undead-One
        addCurvedRoad({18.0f, 12.0f},  {2.0f, 8.0f},    {-14.0f, 4.0f},  false);
        addCurvedRoad({-14.0f, 4.0f},  {-20.0f, -4.0f}, {-26.0f, -16.0f},false);
        // Royal highway around Lowmillburg castle & windmills
        addCurvedRoad({6.0f, -34.0f},  {1.0f, -38.0f},  {11.0f, -38.0f}, true);
    } else if (preset_ == AtlasPreset::HighlandKingdom) {
        addCurvedRoad({-22.0f, -16.0f},{-10.0f, -4.0f},{0.0f, 6.0f},   true);
        addCurvedRoad({0.0f, 6.0f},    {18.0f, 20.0f}, {32.0f, 36.0f}, false);
        addCurvedRoad({0.0f, 6.0f},    {-16.0f, 18.0f},{-30.0f, 28.0f},false);
    }

    // 5. Place Curved Calligraphic Map Banners (Watabou Perilous Shores + Cat Quest region titles)
    if (preset_ == AtlasPreset::PerilousPeninsula) {
        if (seed_ == 1492u) {
            banners_.push_back(CurvedMapBanner{
                "KILDLAND",
                {24.0f, 50.0f}, {40.0f, 54.0f}, {56.0f, 50.0f},
                2.8f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "DUSKWOODS",
                {-24.0f, 26.0f}, {-14.0f, 30.0f}, {-4.0f, 26.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "DRAGON DESERT",
                {10.0f, -4.0f}, {22.0f, 0.0f}, {34.0f, -4.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "THE PERILOUS SHORES",
                {-32.0f, -20.0f}, {-6.0f, -24.0f}, {20.0f, -20.0f},
                2.6f, true, true
            });
        } else {
            std::string realmName  = ProceduralNameGenerator::generateRegionTitle(seed_, BiomeId::LushMeadow);
            std::string forestName = ProceduralNameGenerator::generateRegionTitle(seed_ + 17u, BiomeId::DeciduousWoodland);
            std::string desertName = ProceduralNameGenerator::generateRegionTitle(seed_ + 37u, BiomeId::AridSteppe);

            banners_.push_back(CurvedMapBanner{
                realmName,
                {24.0f, 50.0f}, {40.0f, 54.0f}, {56.0f, 50.0f},
                2.8f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                forestName,
                {-24.0f, 26.0f}, {-14.0f, 30.0f}, {-4.0f, 26.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                desertName,
                {10.0f, -4.0f}, {22.0f, 0.0f}, {34.0f, -4.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "THE PERILOUS SHORES",
                {-32.0f, -20.0f}, {-6.0f, -24.0f}, {20.0f, -20.0f},
                2.6f, true, true
            });
        }
    } else if (preset_ == AtlasPreset::ShatteredArchipelago) {
        banners_.push_back(CurvedMapBanner{
            "THE SHATTERED SEA",
            {-34.0f, -14.0f}, {0.0f, -22.0f}, {34.0f, -14.0f},
            2.6f, true, true
        });
        banners_.push_back(CurvedMapBanner{
            "CORSAIR ATOLL",
            {-18.0f, 8.0f}, {0.0f, 14.0f}, {18.0f, 8.0f},
            1.9f, true, false
        });
    } else {
        banners_.push_back(CurvedMapBanner{
            "SILVERVALE REALM",
            {-28.0f, -6.0f}, {0.0f, 0.0f}, {28.0f, -6.0f},
            2.4f, true, false
        });
    }
}

void VoronoiAtlas::generateOnePageDungeonLayout() {
    // Watabou One-Page Dungeon Generator:
    // Symmetrical/interconnected stone chambers carved out of a cross-hatched rock canvas
    dungeonRooms_ = {
        {0, -10.0f, -32.0f,  10.0f, -16.0f, "ENTRANCE ROTUNDA",   false, false},
        {1,  -4.0f, -16.0f,   4.0f,  -4.0f, "HALL OF ECHOES",     false, false},
        {2, -18.0f,  -4.0f,  18.0f,  16.0f, "PILLARED GREAT HALL",true,  false},
        {3, -34.0f,   0.0f, -18.0f,  14.0f, "DROWNED CRYPT",      true,  false},
        {4,  18.0f,   0.0f,  34.0f,  14.0f, "ALCHEMIST VAULT",    false, false},
        {5,  -4.0f,  16.0f,   4.0f,  26.0f, "SANCTUM STAIR",      false, false},
        {6, -16.0f,  26.0f,  16.0f,  46.0f, "ELDER WYVERN ALTAR", false, true }
    };

    pois_.push_back(ChartPoi{PoiCategory::DungeonPortal,  {  0.0f, -26.0f}, 0.2f, "OVERWORLD EXIT",     "Ascend to Perilous Shores", 1, 1});
    pois_.push_back(ChartPoi{PoiCategory::StandingStones, {-26.0f,   7.0f}, 0.2f, "RUNE OF TIDES",      "Ancient Water Seal",        2, 3});
    pois_.push_back(ChartPoi{PoiCategory::SunkenRuin,     { 26.0f,   7.0f}, 0.2f, "TRANSMUTATION FONT", "Gilded Chest & Relics",     3, 4});
    pois_.push_back(ChartPoi{PoiCategory::BossLair,       {  0.0f,  36.0f}, 0.2f, "THE SUNKEN WYRM",    "Lv. 6 Sanctum Guardian",    4, 6});

    banners_.push_back(CurvedMapBanner{
        "THE SUNKEN SANCTUM",
        {-24.0f, -38.0f}, {0.0f, -42.0f}, {24.0f, -38.0f},
        2.3f, true, false
    });
}

void VoronoiAtlas::ensureRegionBlocks(float centerWorldX, float centerWorldZ, float radius) {
    if (preset_ == AtlasPreset::OnePageDungeon) return;

    constexpr float kBlockSize = 80.0f;
    int minBx = static_cast<int>(std::floor((centerWorldX - radius) / kBlockSize));
    int maxBx = static_cast<int>(std::ceil((centerWorldX + radius) / kBlockSize));
    int minBz = static_cast<int>(std::floor((centerWorldZ - radius) / kBlockSize));
    int maxBz = static_cast<int>(std::ceil((centerWorldZ + radius) / kBlockSize));

    for (int bz = minBz; bz <= maxBz; ++bz) {
        for (int bx = minBx; bx <= maxBx; ++bx) {
            uint64_t key = packBlockCoord(bx, bz);
            if (std::find(generatedBlocks_.begin(), generatedBlocks_.end(), key) == generatedBlocks_.end()) {
                generateMacroBlock(bx, bz);
                generatedBlocks_.push_back(key);
            }
        }
    }
}

void VoronoiAtlas::generateMacroBlock(int bx, int bz) {
    if (bx == 0 && bz == 0) return;

    constexpr float kBlockSize = 80.0f;
    float cx = static_cast<float>(bx) * kBlockSize;
    float cz = static_cast<float>(bz) * kBlockSize;

    uint32_t bHash = hashU32(static_cast<uint32_t>(bx) * 73856093u ^
                             static_cast<uint32_t>(bz) * 19349663u ^
                             seed_);

    float jx = (static_cast<float>((bHash >> 4) & 0xFFu) / 255.0f - 0.5f) * (kBlockSize * 0.44f);
    float jz = (static_cast<float>((bHash >> 12) & 0xFFu) / 255.0f - 0.5f) * (kBlockSize * 0.44f);
    float pox = cx + jx;
    float poz = cz + jz;

    float h = rawElevationAt(pox, poz);
    float m = rawMoistureAt(pox, poz);

    ChartPoi poi{};
    poi.pos = {pox, poz};
    poi.elevation = std::max(0.2f, h);
    poi.seed = bHash;
    poi.questLevel = std::clamp(static_cast<int>(std::sqrt(pox * pox + poz * poz) / 30.0f) + 1, 1, 15);

    bool isWater = (h <= 0.0f);
    if (isWater) {
        if (h > -1.8f && (bHash % 4u == 0u)) {
            poi.category = (bHash % 2u == 0u) ? PoiCategory::SunkenRuin : PoiCategory::Lighthouse;
            poi.name = ProceduralNameGenerator::generateLocaleName(bHash, BiomeId::CoastalShoals, poi.category);
            poi.subtitle = (poi.category == PoiCategory::Lighthouse) ? "Reef Beacon & Anchorage" : "Sunken Coral Obelisk";
            pois_.push_back(poi);
        } else {
            std::string oceanTitle = ProceduralNameGenerator::generateRegionTitle(bHash, BiomeId::OceanDeep);
            banners_.push_back(CurvedMapBanner{
                oceanTitle,
                {pox - 16.0f, poz - 6.0f}, {pox, poz - 12.0f}, {pox + 16.0f, poz - 6.0f},
                2.4f, true, true
            });
            return;
        }
    } else {
        BiomeId biome = classifyBiome(h, m, 8.0f, bHash);
        if (h > 4.5f) {
            poi.category = (bHash % 3u == 0u) ? PoiCategory::AlpineMonastery : PoiCategory::DungeonPortal;
            poi.subtitle = (poi.category == PoiCategory::AlpineMonastery) ? "Highland Astrologer Spire" : "Ancient Mountain Barrow";
        } else if (biome == BiomeId::DeciduousWoodland || biome == BiomeId::TaigaPines) {
            uint32_t sel = bHash % 3u;
            if (sel == 0u) {
                poi.category = PoiCategory::MarketHamlet;
                poi.subtitle = "Forest Outpost & Timber Mills";
            } else if (sel == 1u) {
                poi.category = PoiCategory::Windmill;
                poi.subtitle = "Valley Granary & Windmill";
            } else {
                poi.category = PoiCategory::DungeonPortal;
                poi.subtitle = "Overgrown Skull Crypt";
            }
        } else if (biome == BiomeId::AridSteppe || biome == BiomeId::BadlandsCanyon) {
            poi.category = (bHash % 2u == 0u) ? PoiCategory::CitadelKeep : PoiCategory::SunkenRuin;
            poi.subtitle = (poi.category == PoiCategory::CitadelKeep) ? "Sun-Drenched Sandstone Fortress" : "Buried Desert Pillars";
        } else if (biome == BiomeId::SandyBeach || biome == BiomeId::SaltMarsh) {
            poi.category = (bHash % 2u == 0u) ? PoiCategory::PortHarbor : PoiCategory::Lighthouse;
            poi.subtitle = (poi.category == PoiCategory::PortHarbor) ? "Shoal Harbor & Fishing Pier" : "Coastal Watchtower Beacon";
        } else {
            uint32_t sel = bHash % 4u;
            if (sel == 0u) {
                poi.category = PoiCategory::CitadelKeep;
                poi.subtitle = "Walled Overland Keep";
            } else if (sel == 1u) {
                poi.category = PoiCategory::MarketHamlet;
                poi.subtitle = "Verdant Village & Orchards";
            } else if (sel == 2u) {
                poi.category = PoiCategory::StandingStones;
                poi.subtitle = "Ancient Sunken Megalith Ring";
            } else {
                poi.category = PoiCategory::Windmill;
                poi.subtitle = "Hillside Flour Mill";
            }
        }
        poi.name = ProceduralNameGenerator::generateLocaleName(bHash, biome, poi.category);
        pois_.push_back(poi);

        if ((bHash % 4u == 0u)) {
            std::string rTitle = ProceduralNameGenerator::generateRegionTitle(bHash + 99u, biome);
            banners_.push_back(CurvedMapBanner{
                ProceduralNameGenerator::formatCatQuestTracking(rTitle),
                {pox - 18.0f, poz + 14.0f}, {pox, poz + 20.0f}, {pox + 18.0f, poz + 14.0f},
                2.5f, true, false
            });
        }

        // Connect to nearest existing land POI within trade-route distance
        float bestDist = 130.0f;
        int bestIdx = -1;
        for (size_t i = 0; i + 1 < pois_.size(); ++i) {
            float dx = pox - pois_[i].pos.x;
            float dz = poz - pois_[i].pos.y;
            float d = std::sqrt(dx * dx + dz * dz);
            if (d > 25.0f && d < bestDist && pois_[i].elevation > 0.05f) {
                bestDist = d;
                bestIdx = static_cast<int>(i);
            }
        }
        if (bestIdx >= 0) {
            Vec2f pA = pois_[bestIdx].pos;
            Vec2f pB = poi.pos;
            Vec2f mid{(pA.x + pB.x) * 0.5f, (pA.y + pB.y) * 0.5f};
            float jxRoad = (static_cast<float>((bHash >> 8) & 0x1Fu) / 31.0f - 0.5f) * 16.0f;
            float jzRoad = (static_cast<float>((bHash >> 13) & 0x1Fu) / 31.0f - 0.5f) * 16.0f;
            mid.x += jxRoad;
            mid.y += jzRoad;
            bool highway = (poi.category == PoiCategory::CitadelKeep && pois_[bestIdx].category == PoiCategory::CitadelKeep);
            addCurvedRoad(pA, mid, pB, highway);
        }

        if (h > 3.2f && m > 0.42f && (bHash % 3u == 0u)) {
            float kEps = 1.0f;
            float hGradX = (rawElevationAt(pox + kEps, poz) - h) / kEps;
            float hGradZ = (rawElevationAt(pox, poz + kEps) - h) / kEps;
            float gLen = std::sqrt(hGradX * hGradX + hGradZ * hGradZ);
            if (gLen > 0.05f) {
                Vec2f flow{-hGradX / gLen, -hGradZ / gLen};
                Vec2f rStart{pox, poz};
                Vec2f rMid{pox + flow.x * 18.0f, poz + flow.y * 18.0f};
                Vec2f rMouth{pox + flow.x * 38.0f, poz + flow.y * 38.0f};
                addWindingRiver(rStart, rMid, rMouth, 0.85f, 2.0f);
            }
        }
    }
}

ChartFieldSample VoronoiAtlas::sampleAtKnown(
    float worldX, float worldZ,
    float hC, float mC,
    float gradX, float gradZ,
    float hOff, float mOff
) const noexcept {
    ChartFieldSample s{};
    s.gradX = gradX;
    s.gradZ = gradZ;

    // 1. Evaluate River spline distance and carve river channels (with AABB culling)
    float minRiverDist = 100.0f;
    float activeRiverWidth = 1.0f;
    for (const auto& rv : rivers_) {
        float minX = std::min(rv.p0.x, rv.p1.x) - 6.0f;
        float maxX = std::max(rv.p0.x, rv.p1.x) + 6.0f;
        if (worldX < minX || worldX > maxX) continue;
        float minZ = std::min(rv.p0.y, rv.p1.y) - 6.0f;
        float maxZ = std::max(rv.p0.y, rv.p1.y) + 6.0f;
        if (worldZ < minZ || worldZ > maxZ) continue;

        float t = 0.0f;
        float d = pointToSegmentDist({worldX, worldZ}, rv.p0, rv.p1, t);
        float w = rv.width0 + (rv.width1 - rv.width0) * t;
        if (d - w < minRiverDist - activeRiverWidth) {
            minRiverDist = d;
            activeRiverWidth = w;
        }
    }
    s.sdfRiver = minRiverDist;
    s.riverWidth = activeRiverWidth;

    if (hC > 0.0f && minRiverDist < activeRiverWidth * 2.4f) {
        float carve = std::max(0.0f, 1.0f - (minRiverDist / (activeRiverWidth * 2.4f)));
        hC -= carve * carve * 0.85f;
    }

    // 2. Evaluate Road spline distance & bridge crossings (with AABB culling)
    float minRoadDist = 100.0f;
    float bestRoadArc = 0.0f;
    bool isHighway = false;
    for (const auto& rd : roads_) {
        float minX = std::min(rd.p0.x, rd.p1.x) - 5.0f;
        float maxX = std::max(rd.p0.x, rd.p1.x) + 5.0f;
        if (worldX < minX || worldX > maxX) continue;
        float minZ = std::min(rd.p0.y, rd.p1.y) - 5.0f;
        float maxZ = std::max(rd.p0.y, rd.p1.y) + 5.0f;
        if (worldZ < minZ || worldZ > maxZ) continue;

        float t = 0.0f;
        float d = pointToSegmentDist({worldX, worldZ}, rd.p0, rd.p1, t);
        if (d < minRoadDist) {
            minRoadDist = d;
            bestRoadArc = rd.arcStart + (rd.arcEnd - rd.arcStart) * t;
            isHighway   = rd.isRoyalHighway;
        }
    }
    s.sdfRoad = minRoadDist;
    s.roadArc = isHighway ? bestRoadArc : -bestRoadArc;

    // Flatten terrain slightly under settlements (with AABB culling)
    for (const auto& p : pois_) {
        float dx = worldX - p.pos.x;
        if (std::abs(dx) > 6.0f) continue;
        float dz = worldZ - p.pos.y;
        if (std::abs(dz) > 6.0f) continue;
        float d  = std::sqrt(dx * dx + dz * dz);
        if (d < 5.5f && (p.category == PoiCategory::CitadelKeep ||
                         p.category == PoiCategory::PortHarbor ||
                         p.category == PoiCategory::MarketHamlet)) {
            s.flags |= ChartFlags::IsSettlement;
        }
    }

    // 3. Compute signed coastline distance (sdfCoast)
    float gradMag = std::sqrt(s.gradX * s.gradX + s.gradZ * s.gradZ);
    float metricScale = 1.0f / std::clamp(gradMag, 0.14f, 0.55f);
    s.sdfCoast = hC * metricScale * 2.2f;

    if (hC <= 0.0f) {
        s.elevation = 0.0f;
        s.flags |= ChartFlags::IsWater;
    } else {
        // Authentic Sorcery! 3D Parchment Relief Elevation Scaling:
        // Diverse tiered terrain: low coastal beaches -> gentle rolling meadows/mounds ->
        // terraced plateaus -> soaring alpine mountain crags & jagged ridges
        float baseH = hC;
        float hScaled = 0.0f;
        if (baseH < 1.2f) {
            // Gentle coastal incline from sea level (0.0m to 0.75m)
            float t = baseH / 1.2f;
            hScaled = t * t * 0.75f;
        } else if (baseH < 3.8f) {
            // Sorcery! rolling green mounds & plains (0.75m to 3.2m)
            float t = (baseH - 1.2f) / 2.6f;
            hScaled = 0.75f + (t * (2.0f - t)) * 2.45f;
        } else if (baseH < 5.4f) {
            // Stepped plateaus, badlands & foothills (3.2m to 5.8m)
            float t = (baseH - 3.8f) / 1.6f;
            hScaled = 3.20f + t * 2.60f;
        } else {
            // Soaring alpine peaks & rugged mountain crags (5.8m to 10.5m)
            float t = (baseH - 5.4f);
            hScaled = 5.80f + t * 2.10f + std::pow(std::max(0.0f, t), 1.25f) * 0.85f;
        }

        // River valley carving: rivers flow through naturally carved valleys
        if (minRiverDist < activeRiverWidth * 2.8f) {
            float rNorm = minRiverDist / (activeRiverWidth * 2.8f);
            float riverCarve = (1.0f - rNorm * rNorm) * std::min(1.4f, hScaled * 0.45f);
            hScaled = std::max(0.12f, hScaled - riverCarve);
        }

        s.elevation = hScaled;
    }

    if (minRoadDist < 1.35f && minRiverDist < activeRiverWidth * 1.25f && s.sdfCoast > -1.0f) {
        s.flags |= ChartFlags::IsBridge;
        s.elevation = std::max(s.elevation, 0.45f);
    }

    // 4. Biome classification & watercolor edge-darkening blend
    uint32_t cellSeed = hashU32(static_cast<uint32_t>(std::floor(worldX * 0.1f)) * 73u +
                                static_cast<uint32_t>(std::floor(worldZ * 0.1f)) * 199u + seed_);
    s.biomePrimary = classifyBiome(hC, mC, std::abs(s.sdfCoast), cellSeed);
    s.biomeSecondary = classifyBiome(hOff, mOff, std::abs(s.sdfCoast), cellSeed);
    s.biomeBlend = (s.biomePrimary != s.biomeSecondary) ? 0.85f : 0.15f;

    return s;
}

ChartFieldSample VoronoiAtlas::sampleAt(float worldX, float worldZ) const noexcept {
    if (preset_ == AtlasPreset::OnePageDungeon) {
        ChartFieldSample s{};
        // Evaluate signed distance to carved One-Page Dungeon rooms & corridors
        float minSignedDist = 100.0f; // < 0 inside room floor, > 0 in hatched stone wall
        bool inMoatRoom = false;
        for (const auto& rm : dungeonRooms_) {
            float cx = 0.5f * (rm.minX + rm.maxX);
            float cz = 0.5f * (rm.minZ + rm.maxZ);
            float hx = 0.5f * (rm.maxX - rm.minX);
            float hz = 0.5f * (rm.maxZ - rm.minZ);
            float dx = std::abs(worldX - cx) - hx;
            float dz = std::abs(worldZ - cz) - hz;
            float outsideDist = std::sqrt(std::max(dx, 0.0f) * std::max(dx, 0.0f) +
                                          std::max(dz, 0.0f) * std::max(dz, 0.0f));
            float insideDist = std::min(std::max(dx, dz), 0.0f);
            float d = outsideDist + insideDist;
            if (d < minSignedDist) {
                minSignedDist = d;
                inMoatRoom = rm.hasWaterMoat;
            }
        }

        s.flags = ChartFlags::IsDungeonMode;
        if (minSignedDist <= 0.0f) {
            // Inside carved flagstone room
            s.elevation = 0.25f;
            s.sdfCoast  = -minSignedDist;
            s.biomePrimary   = BiomeId::DungeonFlagstone;
            s.biomeSecondary = BiomeId::DungeonFlagstone;
            if (inMoatRoom && std::abs(worldX) > 5.5f && std::abs(worldX) < 12.5f &&
                worldZ > 1.5f && worldZ < 10.5f) {
                s.elevation = -0.35f;
                s.sdfCoast  = -1.5f;
                s.flags |= ChartFlags::IsWater;
                s.biomePrimary = BiomeId::CoastalShoals;
            }
        } else {
            s.elevation = 1.45f;
            s.sdfCoast  = -minSignedDist;
            s.biomePrimary   = BiomeId::DungeonHatchedWall;
            s.biomeSecondary = BiomeId::DungeonFlagstone;
            s.biomeBlend     = std::exp(-minSignedDist * 0.45f);
        }
        return s;
    }

    constexpr float kEps = 0.65f;
    float hC = rawElevationAt(worldX, worldZ);
    float hX = rawElevationAt(worldX + kEps, worldZ);
    float hZ = rawElevationAt(worldX, worldZ + kEps);
    float gradX = (hX - hC) / kEps;
    float gradZ = (hZ - hC) / kEps;

    float mC = rawMoistureAt(worldX, worldZ);
    float mOff = rawMoistureAt(worldX + 2.8f, worldZ - 2.4f);
    float hOff = rawElevationAt(worldX + 2.8f, worldZ - 2.4f);

    return sampleAtKnown(worldX, worldZ, hC, mC, gradX, gradZ, hOff, mOff);
}

} // namespace eatscharted
