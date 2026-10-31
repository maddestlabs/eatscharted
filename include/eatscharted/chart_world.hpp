#ifndef EATSCHARTED_CHART_WORLD_HPP
#define EATSCHARTED_CHART_WORLD_HPP

#include "eatscharted/chart_types.hpp"
#include "eatscharted/voronoi_atlas.hpp"
#include "eatscharted/ink_canvas.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace eatscharted {

struct MapChunk {
    int cx{0};
    int cz{0};
    float discovery{0.0f}; // 0.0 = uncharted vellum, 1.0 = fully charted watercolor
    std::vector<ChartVertex> terrainVerts;
    std::vector<ChartVertex> bannerVerts;
    std::vector<StandeeInstance> standees;
    std::vector<PointLight> lights;
};

struct ChartStats {
    float atlasGenTimeMs{0.0f};
    uint32_t mapSurfaceTriangles{0};
    uint32_t bannerGlyphTriangles{0};
    uint32_t standeeCount{0};
    uint32_t telegraphCount{0};
    uint32_t pointLightCount{0};
};

struct HeroState {
    Vec3f pos{-4.0f, 0.5f, -1.5f};
    Vec2f vel{0.0f, 0.0f};
    Vec2f facingDir{0.0f, 1.0f};
    bool facingRight{true};
    bool isSailing{false};          // Automatically true when sailing on ocean/bay (sdfCoast < -0.35)
    float dodgeTimer{0.0f};
    float dodgeCooldown{0.0f};
    float attackTimer{0.0f};
    float spellTimer{0.0f};
    float hopTimer{0.0f};           // Movement cycle accumulator (~6 Hz hopping animation)
    float hopArc{0.0f};             // Current vertical bounce displacement factor (0.0 to 1.0)
    int level{1};
    int hp{120};
    int maxHp{120};
    int mana{60};
    int maxMana{60};
    int gold{50};
    int xp{0};
    std::string activeRegionName{"PORT PAWVEN"};
    std::string activeRegionSub{"Royal Cartographer's Harbor"};
};

struct EnemyAgent {
    Vec3f pos{0.0f, 0.0f, 0.0f};
    Vec3f homePos{0.0f, 0.0f, 0.0f};
    uint16_t standeeLayer{InkLayer::StandeeMonsterSlime};
    std::string name{"Ink Slime"};
    float width{2.1f};
    float height{2.1f};
    int hp{45};
    int maxHp{45};
    float telegraphProgress{0.0f};  // > 0 when charging a ground-projected attack circle/cone
    float telegraphRadius{3.6f};
    uint32_t telegraphType{TelegraphType::EnemyCircle};
    float cooldownTimer{1.5f};
    float hitFlashTimer{0.0f};
    bool isBoss{false};
    bool alive{true};
};

struct FloatingNumber {
    Vec3f worldPos{0.0f, 0.0f, 0.0f};
    std::string text;
    RGBA8 color{255, 235, 110, 255};
    float lifetime{0.9f};
    float age{0.0f};
};

struct RaycastHit {
    bool hit{false};
    float distance{0.0f};
    Vec3f position{0.0f, 0.0f, 0.0f};
    BiomeId biome{BiomeId::LushMeadow};
};

struct LocaleBannerState {
    std::string title;
    std::string subtitle;
    float timer{0.0f};
    float duration{2.8f};
    bool active{false};
};

/**
 * Complete 2.5D Living Cartography World State (Watabou Perilous Shores + Cat Quest).
 */
class ChartWorld {
public:
    ChartWorld();

    void clear();

    // High-level declarative world builders
    void buildPerilousPeninsula(uint32_t seed = 1492u);
    void buildShatteredArchipelago(uint32_t seed = 1715u);
    void buildHighlandKingdom(uint32_t seed = 1337u);
    void buildSunscorchedWastes(uint32_t seed = 1066u);
    void buildContinentalPangaea(uint32_t seed = 2026u);
    void buildOnePageDungeon(uint32_t seed = 999u, DungeonTheme theme = DungeonTheme::Auto);
    void generateCustomChart(uint32_t seed, AtlasPreset preset);

    // Watabou One-Page Dungeon API
    void initDungeon(uint32_t seed = 999u, std::string_view title = "THE SUNKEN SANCTUM", std::string_view subtitle = "Watabou One-Page Dungeon");
    void addDungeonRoom(float minX, float minZ, float maxX, float maxZ, std::string_view label = "", bool water = false, bool boss = false);
    void addDungeonCorridor(float x1, float z1, float x2, float z2, float width = 3.2f);
    void addDungeonWaterPool(float minX, float minZ, float maxX, float maxZ);
    void addDungeonTorch(Vec3f pos, Vec3f color = {1.0f, 0.72f, 0.36f}, float radius = 14.0f, float intensity = 1.35f);
    void addDungeonPillar(Vec3f pos);
    void addDungeonChest(Vec3f pos);
    void addDungeonExitPortal(Vec3f pos);
    void addDungeonEnemy(Vec3f pos, std::string_view name, uint16_t layer = InkLayer::StandeeMonsterSlime, int hp = 50, bool isBoss = false, float telegraphRadius = 3.5f, uint32_t telegraphType = TelegraphType::EnemyCircle);
    void buildWatabouDungeon(uint32_t seed = 999u, DungeonTheme theme = DungeonTheme::Auto);
    void buildProceduralDungeon(uint32_t seed = 999u, DungeonTheme theme = DungeonTheme::Auto);
    [[nodiscard]] bool dungeonExplicitDressing() const noexcept { return dungeonExplicitDressing_; }

    // Rebuilds the 3D tabletop parchment mesh, 3D curved calligraphic banners, and static standees
    ChartStats rebuildWorldGeometry(float centerWorldX = 0.0f, float centerWorldZ = 0.0f);

    // Dynamic streaming mesh dirty state for WebGPU buffer uploads
    [[nodiscard]] bool needsMeshUpload() const noexcept { return meshDirty_; }
    void clearMeshDirty() noexcept { meshDirty_ = false; }
    [[nodiscard]] Vec2f gridCenter() const noexcept { return gridCenter_; }
    [[nodiscard]] int activeChunkX() const noexcept { return activeChunkX_; }
    [[nodiscard]] int activeChunkZ() const noexcept { return activeChunkZ_; }
    void fillDiscoveryGrid(int centerCx, int centerCz, float outGrid[16][4]) const noexcept;
    [[nodiscard]] uint32_t discoveredChunkCount() const noexcept;

    // Updates dynamic standees (hero, companion, ships, enemies, slash arcs), telegraphs, and 2D HUD
    void updateSimulation(float dt, uint32_t viewportW, uint32_t viewportH);

    // Player actions (Cat Quest tabletop controls)
    void moveHero(float moveX, float moveZ, bool sprintOrDodge, float dt);
    void triggerMeleeSlash();
    void triggerArcaneSpell();
    bool tryToggleNearbyDungeon();

    // Ephemeral named locale discovery banner (animated fade-in/out across Native & Web)
    void triggerLocaleDiscovery(std::string_view name, std::string_view subtitle = "");
    [[nodiscard]] const LocaleBannerState& localeBanner() const noexcept { return localeBanner_; }
    [[nodiscard]] bool isLocaleBannerActive() const noexcept { return localeBanner_.active; }

    [[nodiscard]] RaycastHit raycast(Vec3f origin, Vec3f dir, float maxDist = 250.0f) const noexcept;

    // Accessors
    [[nodiscard]] const VoronoiAtlas& voronoi() const noexcept { return voronoi_; }
    [[nodiscard]] const InkAtlasData& inkAtlas() const noexcept { return inkAtlas_; }
    [[nodiscard]] const std::vector<ChartVertex>& worldVertices() const noexcept { return worldVerts_; }
    [[nodiscard]] const std::vector<StandeeInstance>& standees() const noexcept { return activeStandees_; }
    [[nodiscard]] const std::vector<CombatTelegraph>& telegraphs() const noexcept { return activeTelegraphs_; }
    [[nodiscard]] const std::vector<PointLight>& pointLights() const noexcept { return pointLights_; }
    [[nodiscard]] const eatsbits::ui::BatchRenderer2D& overlay2D() const noexcept { return overlay2D_; }

    [[nodiscard]] HeroState& hero() noexcept { return hero_; }
    [[nodiscard]] const HeroState& hero() const noexcept { return hero_; }

    [[nodiscard]] CameraConfig& camera() noexcept { return camera_; }
    [[nodiscard]] const CameraConfig& camera() const noexcept { return camera_; }

    [[nodiscard]] AtmosphereConfig& atmosphere() noexcept { return atmosphere_; }
    [[nodiscard]] const AtmosphereConfig& atmosphere() const noexcept { return atmosphere_; }
    void setAtmosphere(const AtmosphereConfig& atm) noexcept { atmosphere_ = atm; }

    void setTimeOfDay(float t) noexcept { atmosphere_.timeOfDay = t; }
    [[nodiscard]] float timeOfDay() const noexcept { return atmosphere_.timeOfDay; }
    [[nodiscard]] bool useDayNightCycle() const noexcept { return useDayNightCycle_; }
    void setUseDayNightCycle(bool enable) noexcept { useDayNightCycle_ = enable; }
    [[nodiscard]] float dayCycleSpeed() const noexcept { return dayCycleSpeed_; }
    void setDayCycleSpeed(float speed) noexcept { dayCycleSpeed_ = speed; }

    static constexpr float kChunkSize{32.0f};
    static constexpr int kChunkRadius{2}; // 5x5 chunk grid = 160m x 160m active window (25 chunks)

    void addCustomStandee(Vec3f pos, float w, float h, uint16_t layer, uint32_t flags = StandeeFlags::PopUpTilt);
    void addCustomBanner(const std::string& text, Vec2f p0, Vec2f p1, Vec2f p2, float letterSize = 2.2f);
    void addPointLight(Vec3f pos, float radius, Vec3f color, float intensity);

private:
    void updateChunkStreaming(float heroX, float heroZ, bool forceRebuild = false);
    void servicePendingChunkBakes();
    void assembleActiveMesh();
    MapChunk bakeChunk(int cx, int cz);
    void emitBannerToChunk(const CurvedMapBanner& b, std::vector<ChartVertex>& outVerts,
                           float minX, float maxX, float minZ, float maxZ);

    int activeChunkX_{-999999};
    int activeChunkZ_{-999999};
    int targetChunkX_{-999999};
    int targetChunkZ_{-999999};
    std::vector<std::pair<int, int>> pendingBakeChunks_;
    std::unordered_map<uint64_t, MapChunk> chunkCache_;

    void buildDynamicStandeesAndTelegraphs();
    void buildHudAndMapOverlay(uint32_t viewportW, uint32_t viewportH);

    Vec2f gridCenter_{0.0f, 0.0f};
    bool meshDirty_{false};

    VoronoiAtlas voronoi_;
    InkAtlasData inkAtlas_;
    bool atlasGenerated_{false};
    float atlasGenTimeMs_{0.0f};

    std::vector<ChartVertex> worldVerts_;
    uint32_t mapSurfaceTriCount_{0};
    uint32_t bannerGlyphTriCount_{0};

    std::vector<StandeeInstance> staticStandees_;
    std::vector<StandeeInstance> activeStandees_;
    std::vector<CombatTelegraph> activeTelegraphs_;
    std::vector<PointLight> pointLights_;
    std::vector<PointLight> staticChunkLights_;
    std::vector<PointLight> customPointLights_;
    std::vector<CurvedMapBanner> customBanners_;
    std::vector<StandeeInstance> customStandees_;

    HeroState hero_;
    std::vector<EnemyAgent> enemies_;
    std::vector<FloatingNumber> floatingNumbers_;

    CameraConfig camera_;
    AtmosphereConfig atmosphere_;
    bool useDayNightCycle_{true};
    bool dungeonExplicitDressing_{false};
    float dayCycleSpeed_{1.0f / 180.0f}; // 1 full cycle per 3 minutes (180 seconds)
    float simTimeSec_{0.0f};
    uint32_t overworldReturnSeed_{1492u};
    AtlasPreset overworldReturnPreset_{AtlasPreset::PerilousPeninsula};
    Vec3f overworldReturnPos_{-4.0f, 0.5f, -1.5f};

    LocaleBannerState localeBanner_;
    std::string lastDiscoveredLocale_;

    eatsbits::ui::BatchRenderer2D overlay2D_;
};

} // namespace eatscharted

#endif // EATSCHARTED_CHART_WORLD_HPP
