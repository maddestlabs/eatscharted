#include "eatscharted/voronoi_atlas.hpp"
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
        float spine = std::exp(-(spineDz * spineDz)) * ridge * 0.95f;

        float hNorm = mainlandBias - bayCarve + isl1 + isl2 + spine + detail * 0.48f;
        return hNorm * 6.5f;
    } else if (preset_ == AtlasPreset::ShatteredArchipelago) {
        // Multi-island nautical chart with central lagoon & surrounding coral/rocky islands
        float r = std::sqrt(qx * qx + qz * qz);
        float ring = std::exp(-std::pow((r - 36.0f) / 22.0f, 2.0f)) * 0.55f;
        float centerIsle = std::exp(-(qx * qx + qz * qz) / (18.0f * 18.0f)) * 0.65f;
        float archipelago = fbmNoise(qx * 0.028f, qz * 0.028f, seed_ + 113u, 5) * 0.85f;
        float hNorm = ring + centerIsle + archipelago - 0.18f;
        return hNorm * 6.2f;
    } else {
        // HighlandKingdom: Twin mountain ranges flanking a lush central river valley
        float valleyDist = std::abs(qx - std::sin(qz * 0.035f) * 18.0f);
        float valleyProfile = std::clamp((valleyDist - 10.0f) / 42.0f, -0.25f, 1.1f);
        float southernCoast = std::clamp((qz + 58.0f) / 25.0f, -0.6f, 0.4f);
        float hNorm = valleyProfile * 0.65f + southernCoast * 0.45f + detail * 0.42f + ridge * 0.35f;
        return hNorm * 7.2f;
    }
}

float VoronoiAtlas::rawMoistureAt(float wx, float wz) const noexcept {
    float m = 0.52f + fbmNoise(wx * 0.021f + 31.0f, wz * 0.021f - 19.0f, seed_ + 211u, 4) * 0.48f;
    return std::clamp(m, 0.0f, 1.0f);
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
    auto addWindingRiver = [&](Vec2f start, Vec2f mid, Vec2f mouth, float startWidth, float endWidth) {
        constexpr int kSteps = 18;
        Vec2f prev = start;
        for (int i = 1; i <= kSteps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(kSteps);
            float u = 1.0f - t;
            Vec2f pt{
                u * u * start.x + 2.0f * u * t * mid.x + t * t * mouth.x,
                u * u * start.y + 2.0f * u * t * mid.y + t * t * mouth.y
            };
            // Add organic meander perpendicular to flow
            float meander = std::sin(t * 12.56f + static_cast<float>(seed_ % 7u)) * (1.0f - t * 0.5f) * 2.8f;
            pt.x += meander;
            float w0 = startWidth + (endWidth - startWidth) * (static_cast<float>(i - 1) / static_cast<float>(kSteps));
            float w1 = startWidth + (endWidth - startWidth) * t;
            rivers_.push_back(RiverSegment{prev, pt, w0, w1});
            prev = pt;
        }
    };

    if (preset_ == AtlasPreset::PerilousPeninsula) {
        addWindingRiver({-8.0f, 56.0f}, {-14.0f, 22.0f}, {-12.0f, -6.0f}, 0.9f, 2.5f);
        addWindingRiver({28.0f, 48.0f}, {10.0f, 24.0f},  {-12.0f, 12.0f}, 0.8f, 1.5f);
    } else if (preset_ == AtlasPreset::HighlandKingdom) {
        addWindingRiver({-4.0f, 70.0f}, {2.0f, 10.0f},   {-6.0f, -55.0f}, 1.2f, 3.2f);
        addWindingRiver({-44.0f, 38.0f}, {-20.0f, 14.0f},{-1.0f, 4.0f},   0.8f, 1.6f);
    } else {
        addWindingRiver({4.0f, 16.0f},  {0.0f, 4.0f},    {-10.0f, -12.0f},0.7f, 1.6f);
    }

    // 3. Place Cat Quest & Perilous Shores Landmarks / Settlements (POIs)
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
        addPoi(PoiCategory::PortHarbor,      -4.0f,  -4.0f, "PORT PAWVEN",       "Royal Cartographer's Harbor", 1);
        addPoi(PoiCategory::CitadelKeep,     16.0f,  16.0f, "CITADEL OF AETHEL", "Walled Bastion of the Realm", 3);
        addPoi(PoiCategory::MarketHamlet,   -24.0f,  18.0f, "OAKENFORD HAMLET",  "Timber Mill & Crossroads",    2);
        addPoi(PoiCategory::Windmill,         4.0f,   8.0f, "GOLDENSPUR MILL",   "Wind-Swept Grain Terraces",   1);
        addPoi(PoiCategory::DungeonPortal,   34.0f,  32.0f, "SUNKEN SANCTUM",    "Lv. 4 Ancient Crypt Portal",  4);
        addPoi(PoiCategory::AlpineMonastery, -8.0f,  46.0f, "STARFALL ABBEY",    "Highland Astrologer Spire",   5);
        addPoi(PoiCategory::Lighthouse,     -34.0f, -26.0f, "BEACON ISLE",       "Lantern of the Outer Shoals", 2);
        addPoi(PoiCategory::SunkenRuin,      26.0f, -32.0f, "DROWNED OBELISK",   "Shattered Sea-King Ruins",    6);
        addPoi(PoiCategory::StandingStones, -32.0f,  38.0f, "WHISPERING STONES", "Leyline Pact Circle",         3);
        addPoi(PoiCategory::BossLair,        42.0f,   4.0f, "WYVERN'S CRAG",     "Lv. 8 Elder Sea-Wyvern Lair", 8);
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
    auto addCurvedRoad = [&](Vec2f a, Vec2f ctrl, Vec2f b, bool isHighway) {
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
    };

    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // Royal double-cased ink highway: Port Pawven -> Goldenspur Mill -> Citadel of Aethel
        addCurvedRoad({-4.0f, -4.0f},  {2.0f,  6.0f},  {16.0f, 16.0f}, true);
        // Highway west to Oakenford Hamlet
        addCurvedRoad({-4.0f, -4.0f},  {-16.0f, 6.0f}, {-24.0f, 18.0f}, true);
        // Dashed explorer trails to Sunken Sanctum, Starfall Abbey, and Wyvern's Crag
        addCurvedRoad({16.0f, 16.0f},  {26.0f, 22.0f}, {34.0f, 32.0f}, false);
        addCurvedRoad({-24.0f, 18.0f}, {-18.0f, 34.0f},{-8.0f, 46.0f}, false);
        addCurvedRoad({16.0f, 16.0f},  {30.0f, 12.0f}, {42.0f,  4.0f}, false);
        addCurvedRoad({-24.0f, 18.0f}, {-30.0f, 28.0f},{-32.0f, 38.0f}, false);
    } else if (preset_ == AtlasPreset::HighlandKingdom) {
        addCurvedRoad({-22.0f, -16.0f},{-10.0f, -4.0f},{0.0f, 6.0f},   true);
        addCurvedRoad({0.0f, 6.0f},    {18.0f, 20.0f}, {32.0f, 36.0f}, false);
        addCurvedRoad({0.0f, 6.0f},    {-16.0f, 18.0f},{-30.0f, 28.0f},false);
    }

    // 5. Place Curved Calligraphic Map Banners (Watabou Perilous Shores + Cat Quest region titles)
    if (preset_ == AtlasPreset::PerilousPeninsula) {
        banners_.push_back(CurvedMapBanner{
            "THE PERILOUS SHORES",
            {-38.0f, -18.0f}, {-10.0f, -26.0f}, {20.0f, -16.0f},
            2.6f, true, true
        });
        banners_.push_back(CurvedMapBanner{
            "WHISPERING WOODS",
            {-36.0f, 24.0f}, {-18.0f, 30.0f}, {0.0f, 24.0f},
            1.85f, true, false
        });
        banners_.push_back(CurvedMapBanner{
            "DRAGONBONE RIDGE",
            {-14.0f, 52.0f}, {12.0f, 58.0f}, {38.0f, 48.0f},
            2.15f, true, false
        });
        banners_.push_back(CurvedMapBanner{
            "CROWNLANDS",
            {2.0f, 10.0f}, {16.0f, 13.0f}, {30.0f, 9.0f},
            1.75f, true, false
        });
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

ChartFieldSample VoronoiAtlas::sampleAt(float worldX, float worldZ) const noexcept {
    ChartFieldSample s{};

    if (preset_ == AtlasPreset::OnePageDungeon) {
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
            s.sdfCoast  = -minSignedDist; // Distance from room wall!
            s.biomePrimary   = BiomeId::DungeonFlagstone;
            s.biomeSecondary = BiomeId::DungeonFlagstone;
            // Optional central water pool in the Pillared Great Hall
            if (inMoatRoom && std::abs(worldX) > 5.5f && std::abs(worldX) < 12.5f &&
                worldZ > 1.5f && worldZ < 10.5f) {
                s.elevation = -0.35f;
                s.sdfCoast  = -1.5f;
                s.flags |= ChartFlags::IsWater;
                s.biomePrimary = BiomeId::CoastalShoals;
            }
        } else {
            // Raised cross-hatched solid rock perimeter (Watabou's signature hatched dungeon walls)
            s.elevation = 1.45f;
            s.sdfCoast  = -minSignedDist;
            s.biomePrimary   = BiomeId::DungeonHatchedWall;
            s.biomeSecondary = BiomeId::DungeonFlagstone;
            s.biomeBlend     = std::exp(-minSignedDist * 0.45f);
        }
        return s;
    }

    // 1. Evaluate continuous elevation and analytical finite-difference slope gradient (∇h)
    constexpr float kEps = 0.65f;
    float hC = rawElevationAt(worldX, worldZ);
    float hX = rawElevationAt(worldX + kEps, worldZ);
    float hZ = rawElevationAt(worldX, worldZ + kEps);
    s.gradX = (hX - hC) / kEps;
    s.gradZ = (hZ - hC) / kEps;

    // 2. Evaluate River spline distance and carve river channels
    float minRiverDist = 100.0f;
    float activeRiverWidth = 1.0f;
    for (const auto& rv : rivers_) {
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

    // 3. Evaluate Road spline distance & bridge crossings
    float minRoadDist = 100.0f;
    float bestRoadArc = 0.0f;
    bool isHighway = false;
    for (const auto& rd : roads_) {
        float t = 0.0f;
        float d = pointToSegmentDist({worldX, worldZ}, rd.p0, rd.p1, t);
        if (d < minRoadDist) {
            minRoadDist = d;
            bestRoadArc = rd.arcStart + (rd.arcEnd - rd.arcStart) * t;
            isHighway   = rd.isRoyalHighway;
        }
    }
    s.sdfRoad = minRoadDist;
    s.roadArc = isHighway ? bestRoadArc : -bestRoadArc; // sign encodes highway vs dashed trail

    // Flatten terrain slightly under settlements
    for (const auto& p : pois_) {
        float dx = worldX - p.pos.x;
        float dz = worldZ - p.pos.y;
        float d  = std::sqrt(dx * dx + dz * dz);
        if (d < 5.5f && (p.category == PoiCategory::CitadelKeep ||
                         p.category == PoiCategory::PortHarbor ||
                         p.category == PoiCategory::MarketHamlet)) {
            s.flags |= ChartFlags::IsSettlement;
        }
    }

    // 4. Compute signed coastline distance (sdfCoast)
    // Scale elevation near shoreline by local gradient to obtain accurate metric distance for concentric water-lining rings
    float gradMag = std::sqrt(s.gradX * s.gradX + s.gradZ * s.gradZ);
    float metricScale = 1.0f / std::clamp(gradMag, 0.14f, 0.55f);
    s.sdfCoast = hC * metricScale * 2.2f;

    // Tabletop height compression: keep land gently sculpted so 2.5D standees & map typography pop cleanly
    if (hC <= 0.0f) {
        s.elevation = 0.0f;
        s.flags |= ChartFlags::IsWater;
    } else {
        // Gentle tabletop relief (0.0m at beach up to ~5.2m on alpine peaks)
        s.elevation = std::pow(hC * 0.35f, 1.12f);
    }

    // Check if road crosses a river -> flag as Bridge!
    if (minRoadDist < 1.35f && minRiverDist < activeRiverWidth * 1.25f && s.sdfCoast > -1.0f) {
        s.flags |= ChartFlags::IsBridge;
        s.elevation = std::max(s.elevation, 0.32f);
    }

    // 5. Biome classification & watercolor edge-darkening blend
    float moisture = rawMoistureAt(worldX, worldZ);
    uint32_t cellSeed = hashU32(static_cast<uint32_t>(std::floor(worldX * 0.1f)) * 73u +
                                static_cast<uint32_t>(std::floor(worldZ * 0.1f)) * 199u + seed_);
    s.biomePrimary = classifyBiome(hC, moisture, std::abs(s.sdfCoast), cellSeed);

    // Sample slightly offset point to detect biome transitions for watercolor pigment edge-pooling
    float mOff = rawMoistureAt(worldX + 2.8f, worldZ - 2.4f);
    float hOff = rawElevationAt(worldX + 2.8f, worldZ - 2.4f);
    s.biomeSecondary = classifyBiome(hOff, mOff, std::abs(s.sdfCoast), cellSeed);
    s.biomeBlend = (s.biomePrimary != s.biomeSecondary) ? 0.85f : 0.15f;

    return s;
}

} // namespace eatscharted
