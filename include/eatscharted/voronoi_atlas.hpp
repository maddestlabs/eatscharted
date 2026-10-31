#ifndef EATSCHARTED_VORONOI_ATLAS_HPP
#define EATSCHARTED_VORONOI_ATLAS_HPP

#include "eatscharted/chart_types.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>

namespace eatscharted {

enum class AtlasPreset : uint8_t {
    PerilousPeninsula    = 0, // Classic Watabou Perilous Shores coastal peninsula, bay, islands & ridge
    ShatteredArchipelago = 1, // Naval island chain, sea stacks, coral shoals & pirate coves
    HighlandKingdom      = 2, // Deep river valley, royal trade roads, twin citadels & alpine peaks
    OnePageDungeon       = 3, // Watabou One-Page Dungeon cross-hatched subterranean sanctum
    SunscorchedWastes    = 4, // Arid dunes, sandstone mesas, desert canyons, palm oasis & ancient tombs
    ContinentalPangaea   = 5  // Seamless unified overland continent combining all biomes
};

/**
 * Parameterized Province Specification for modular biome placement and continental blending.
 */
struct ProvinceSpec {
    Vec2f center{0.0f, 0.0f};
    float radius{96.0f};
    float blendWidth{24.0f};
};

enum class PoiCategory : uint8_t {
    CitadelKeep     = 0,
    PortHarbor      = 1,
    MarketHamlet    = 2,
    AlpineMonastery = 3,
    DungeonPortal   = 4,
    SunkenRuin      = 5,
    Lighthouse      = 6,
    Windmill        = 7,
    StandingStones  = 8,
    BossLair        = 9
};

struct VoronoiRegionCell {
    int id{0};
    Vec2f site{0.0f, 0.0f};
    float elevation{0.0f};      // -1.0 (deep sea) .. +1.0 (high peak)
    float moisture{0.5f};       // 0.0 (arid) .. 1.0 (wetland)
    float coastDist{0.0f};      // Signed distance to shoreline in meters
    BiomeId biome{BiomeId::LushMeadow};
    bool isWater{false};
    int downhillNeighbor{-1};
    std::vector<int> neighbors;
};

struct RiverSegment {
    Vec2f p0{0.0f, 0.0f};
    Vec2f p1{0.0f, 0.0f};
    float width0{1.2f};
    float width1{1.8f};
    float minX{0.0f};
    float minZ{0.0f};
    float maxX{0.0f};
    float maxZ{0.0f};
};

struct RoadSegment {
    Vec2f p0{0.0f, 0.0f};
    Vec2f p1{0.0f, 0.0f};
    float arcStart{0.0f};
    float arcEnd{1.0f};
    bool isRoyalHighway{true};  // true = double-cased ink highway, false = dashed trail
    float minX{0.0f};
    float minZ{0.0f};
    float maxX{0.0f};
    float maxZ{0.0f};
};

struct ChartPoi {
    PoiCategory category{PoiCategory::MarketHamlet};
    Vec2f pos{0.0f, 0.0f};
    float elevation{1.0f};
    std::string name;
    std::string subtitle;
    uint32_t seed{0};
    int questLevel{1};
};

/**
 * Curved Calligraphic Map Banner laid out along a Quadratic Bezier spline (P0 -> P1 -> P2)
 * directly on the parchment map surface (Watabou Perilous Shores + Cat Quest style).
 */
struct CurvedMapBanner {
    std::string text;
    Vec2f p0{0.0f, 0.0f};
    Vec2f p1{0.0f, 0.0f};
    Vec2f p2{0.0f, 0.0f};
    float letterSize{2.4f};     // World-space height in meters
    bool isRegionHeader{true};  // true = large arched region title, false = town ribbon banner
    bool isWaterTitle{false};   // Italicized/spaced sea & bay nomenclature
};

enum class DungeonTheme : uint8_t {
    Auto            = 0,
    SunkenSanctum   = 1, // Flooded antique teal cistern, water pools, wyrm boss
    WhisperingCrypt = 2, // Dry stone ossuary, sarcophagi, undead spectres
    MoltenVault     = 3, // Magma basins, smoldering braziers, fire drake
    GildedReliquary = 4  // Grand treasury, golden altar, alchemical sentinels
};

struct DungeonRoom {
    int id{0};
    float minX{0.0f};
    float minZ{0.0f};
    float maxX{0.0f};
    float maxZ{0.0f};
    std::string label;
    bool hasWaterMoat{false};
    bool isBossRoom{false};
};

struct DungeonCorridor {
    Vec2f p0{0.0f, 0.0f};
    Vec2f p1{0.0f, 0.0f};
    float halfWidth{1.6f};
};

struct DungeonWaterPool {
    float minX{0.0f};
    float minZ{0.0f};
    float maxX{0.0f};
    float maxZ{0.0f};
};

/**
 * Procedural Dual-Mesh Cartographic World Generator (Watabou's Perilous Shores + One-Page Dungeon).
 */
class VoronoiAtlas {
public:
    VoronoiAtlas() = default;

    void generate(uint32_t seed = 1492u, AtlasPreset preset = AtlasPreset::PerilousPeninsula);

    [[nodiscard]] ChartFieldSample sampleAt(float worldX, float worldZ) const noexcept;

    // Fast-path evaluation when elevation, moisture, and local finite-difference slope are precomputed
    [[nodiscard]] ChartFieldSample sampleAtKnown(
        float worldX, float worldZ,
        float hC, float mC,
        float gradX, float gradZ,
        float hOff = 0.0f, float mOff = 0.0f
    ) const noexcept;

    [[nodiscard]] float rawElevation(float wx, float wz) const noexcept { return rawElevationAt(wx, wz); }
    [[nodiscard]] float rawMoisture(float wx, float wz) const noexcept { return rawMoistureAt(wx, wz); }

    // Parameterized province evaluation (allowing biomes to be offset anywhere in continental space)
    [[nodiscard]] float rawElevationDesert(float wx, float wz, Vec2f center = {0.0f, 0.0f}, float radius = 96.0f) const noexcept;
    [[nodiscard]] float rawMoistureDesert(float wx, float wz, Vec2f center = {0.0f, 0.0f}, float radius = 96.0f) const noexcept;
    [[nodiscard]] float rawElevationContinental(float wx, float wz) const noexcept;
    [[nodiscard]] float rawMoistureContinental(float wx, float wz) const noexcept;

    [[nodiscard]] uint32_t seed() const noexcept { return seed_; }
    [[nodiscard]] AtlasPreset preset() const noexcept { return preset_; }
    [[nodiscard]] bool isDungeonMode() const noexcept { return preset_ == AtlasPreset::OnePageDungeon; }
    [[nodiscard]] DungeonTheme dungeonTheme() const noexcept { return dungeonTheme_; }

    [[nodiscard]] static uint32_t deriveSubSeed(uint32_t masterSeed, uint32_t featureId, uint32_t floor = 1) noexcept {
        uint32_t s = masterSeed ^ (featureId * 0x9E3779B9u) ^ (floor * 0x85EBCA6Bu);
        s ^= s >> 16;
        s *= 0x85ebca6bu;
        s ^= s >> 13;
        s *= 0xc2b2ae35u;
        s ^= s >> 16;
        return s;
    }

    [[nodiscard]] const std::vector<VoronoiRegionCell>& cells() const noexcept { return cells_; }
    [[nodiscard]] const std::vector<RiverSegment>& rivers() const noexcept { return rivers_; }
    [[nodiscard]] const std::vector<RoadSegment>& roads() const noexcept { return roads_; }
    [[nodiscard]] const std::vector<ChartPoi>& pois() const noexcept { return pois_; }
    [[nodiscard]] const std::vector<CurvedMapBanner>& banners() const noexcept { return banners_; }
    [[nodiscard]] const std::vector<DungeonRoom>& dungeonRooms() const noexcept { return dungeonRooms_; }
    [[nodiscard]] const std::vector<DungeonCorridor>& dungeonCorridors() const noexcept { return dungeonCorridors_; }
    [[nodiscard]] const std::vector<DungeonWaterPool>& dungeonWaterPools() const noexcept { return dungeonWaterPools_; }

    void clearDungeon();
    void addDungeonRoom(int id, float minX, float minZ, float maxX, float maxZ, std::string_view label = "", bool hasWaterMoat = false, bool isBossRoom = false);
    void addDungeonCorridor(float x1, float z1, float x2, float z2, float width = 3.2f);
    void addDungeonWaterPool(float minX, float minZ, float maxX, float maxZ);
    void generateWatabouProcedural(uint32_t seed, DungeonTheme theme = DungeonTheme::Auto);
    void setPreset(AtlasPreset preset) noexcept { preset_ = preset; }
    void setSeed(uint32_t seed) noexcept { seed_ = seed; }

    // Hydrologically traces a river from a high-altitude or spring source downhill to sea level
    void traceHydrologicalRiver(Vec2f source, float startWidth = 1.0f, float terminalElevation = 0.02f, float maxReach = 380.0f);

    // Dynamically seeds and streams macro-blocks/chunks as the hero explores across the infinite map
    void ensureRegionBlocks(float centerWorldX, float centerWorldZ, float radius = 96.0f);

private:
    void generateOverworldGraph();
    void generateOnePageDungeonLayout();
    void generateMacroBlock(int bx, int bz);
    void addCurvedRoad(Vec2f a, Vec2f ctrl, Vec2f b, bool isHighway);
    void addWindingRiver(Vec2f start, Vec2f mid, Vec2f mouth, float startWidth, float endWidth);
    void indexRiverSegment(uint32_t segIdx);

    [[nodiscard]] float rawElevationAt(float wx, float wz) const noexcept;
    [[nodiscard]] float rawMoistureAt(float wx, float wz) const noexcept;

    static constexpr float kRiverSpatialCell = 32.0f;

    uint32_t seed_{1492u};
    AtlasPreset preset_{AtlasPreset::PerilousPeninsula};
    DungeonTheme dungeonTheme_{DungeonTheme::SunkenSanctum};
    std::vector<VoronoiRegionCell> cells_;
    std::vector<RiverSegment> rivers_;
    std::unordered_map<uint64_t, std::vector<uint32_t>> riverSpatialGrid_;
    std::vector<RoadSegment> roads_;
    std::vector<ChartPoi> pois_;
    std::vector<CurvedMapBanner> banners_;
    std::vector<DungeonRoom> dungeonRooms_;
    std::vector<DungeonCorridor> dungeonCorridors_;
    std::vector<DungeonWaterPool> dungeonWaterPools_;
    std::unordered_set<uint64_t> generatedBlocks_;
};

} // namespace eatscharted

#endif // EATSCHARTED_VORONOI_ATLAS_HPP
