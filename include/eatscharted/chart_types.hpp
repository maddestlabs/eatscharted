#ifndef EATSCHARTED_CHART_TYPES_HPP
#define EATSCHARTED_CHART_TYPES_HPP

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace eatscharted {

struct Vec2f {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2f operator+(Vec2f o) const noexcept { return {x + o.x, y + o.y}; }
    constexpr Vec2f operator-(Vec2f o) const noexcept { return {x - o.x, y - o.y}; }
    constexpr Vec2f operator*(float s) const noexcept { return {x * s, y * s}; }
    constexpr Vec2f operator/(float s) const noexcept { return {x / s, y / s}; }
};

inline constexpr Vec2f operator*(float s, Vec2f v) noexcept { return {v.x * s, v.y * s}; }

struct Vec3f {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Vec4f {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};
};

struct RGBA8 {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    [[nodiscard]] constexpr uint32_t pack() const noexcept {
        return static_cast<uint32_t>(r) |
               (static_cast<uint32_t>(g) << 8u) |
               (static_cast<uint32_t>(b) << 16u) |
               (static_cast<uint32_t>(a) << 24u);
    }

    static constexpr RGBA8 unpack(uint32_t v) noexcept {
        return RGBA8{
            static_cast<uint8_t>(v & 0xFFu),
            static_cast<uint8_t>((v >> 8u) & 0xFFu),
            static_cast<uint8_t>((v >> 16u) & 0xFFu),
            static_cast<uint8_t>((v >> 24u) & 0xFFu)
        };
    }
};

/**
 * Cartographic Biome Identifiers (Watabou Perilous Shores + Cat Quest palette).
 */
enum class BiomeId : uint8_t {
    OceanDeep         = 0,  // Deep charted sea with compass rhumb lines & wave dashes
    CoastalShoals     = 1,  // Shallow turquoise/parchment coast with concentric ink water-lining
    SandyBeach        = 2,  // Stippled warm sandbar & coastal dunes
    SaltMarsh         = 3,  // Ink-tufted coastal wetlands & brackish estuaries
    LushMeadow        = 4,  // Watercolor sage-olive rolling plains
    DeciduousWoodland = 5,  // Deep moss forest floor under clumped canopy standees
    TaigaPines        = 6,  // Cool slate-pine northern highlands
    AridSteppe        = 7,  // Ochre-gold dry savanna & scrubland
    BadlandsCanyon    = 8,  // Rust-sienna mesas & rocky scree
    AlpinePeak        = 9,  // High hachured mountain ridges & snow-inked crests
    CalamityBlight    = 10, // Ashen violet-charcoal ruptured leyline wastes
    DungeonFlagstone  = 11, // One-Page Dungeon gridded flagstone floor
    DungeonHatchedWall= 12  // One-Page Dungeon cross-hatched solid rock perimeter
};

/**
 * Procedural InkCanvas Texture2DArray Layer IDs (64x64 hand-inked standees & map icons).
 */
namespace InkLayer {
    constexpr uint16_t ParchmentGrain      = 0;
    constexpr uint16_t CobblePlaza         = 1;
    constexpr uint16_t TimberBridge        = 2;
    constexpr uint16_t DungeonFloorTile    = 3;
    constexpr uint16_t DungeonWallHatch    = 4;
    constexpr uint16_t InkCalligraphyInk   = 5;  // Solid/gold-rimmed ink for 3D map surface banners

    // 2.5D Volumetric Standees (Realms-style 2.5D normal-mapped props & foliage)
    constexpr uint16_t StandeeRockPillar   = 6;  // Faceted basalt/granite crag pillar (replaces land mountain peak)
    constexpr uint16_t StandeeRockOutcrop  = 7;  // Secondary mossy boulder / rocky outcrop cluster
    constexpr uint16_t StandeeSeaStack     = 8;  // Offshore jagged basalt sea stack with seafoam base
    constexpr uint16_t StandeeForestOak    = 9;  // Realms-style 2.5D multi-tiered deciduous oak canopy & trunk
    constexpr uint16_t StandeeForestPine   = 10; // Realms-style 2.5D layered boreal pine needles & trunk
    constexpr uint16_t StandeeForestPalm   = 11; // Coastal curved palm & lush tropical frond cluster
    constexpr uint16_t StandeeGrassPatch   = 12; // Realms-style 2.5D lush grass blades & wildflower tuft

    // Legacy aliases for backwards-compatibility
    constexpr uint16_t StandeeMountainPeak = StandeeRockPillar;
    constexpr uint16_t StandeeMountainRidge= StandeeRockOutcrop;
    constexpr uint16_t StandeeMarshReeds   = StandeeGrassPatch;

    // 2.5D Tabletop Settlement & Landmark Standees (Cat Quest style)
    constexpr uint16_t StandeeTownKeep     = 13; // Walled citadel keep with banners
    constexpr uint16_t StandeeHamletCottage= 14; // Gabled village house cluster
    constexpr uint16_t StandeeWindmill     = 15; // Rural timber windmill
    constexpr uint16_t StandeeLighthouse   = 16; // Coastal beacon tower
    constexpr uint16_t StandeeMonastery    = 17; // Highland spire sanctuary
    constexpr uint16_t StandeePortHarbor   = 18; // Harbor pier & anchor emblem
    constexpr uint16_t StandeeDungeonArch  = 19; // Cave maw / stone dungeon portal arch
    constexpr uint16_t StandeeSunkenRuin   = 20; // Broken columns & ancient obelisk
    constexpr uint16_t StandeeStandingStone= 21; // Glowing rune monolith
    constexpr uint16_t StandeeSignpost     = 22; // Crossroads wooden signpost
    constexpr uint16_t StandeeTreasureChest= 23; // Inked & gilded treasure chest
    constexpr uint16_t StandeeCampfire     = 24; // Flickering adventurer campfire

    // 2.5D Living Puppets, Ships & Combat FX
    constexpr uint16_t StandeeHeroCat      = 25; // Cat Quest style armored feline adventurer
    constexpr uint16_t StandeeHeroCompanion= 26; // Floating arcane spirit / mage companion
    constexpr uint16_t StandeeShipCaravel  = 27; // Inked caravel sailing ship with pennant
    constexpr uint16_t StandeeMonsterSlime = 28; // Ink-blotted arcane slime
    constexpr uint16_t StandeeMonsterBandit= 29; // Highwayman / goblin marauder
    constexpr uint16_t StandeeMonsterWyvern= 30; // Perilous Shores sea serpent / wyvern boss
    constexpr uint16_t StandeeSlashArc     = 31; // Dynamic sword/claw slash crescent

    constexpr uint32_t kCount              = 32;
    constexpr uint32_t kTileSize           = 64;
} // namespace InkLayer

[[nodiscard]] constexpr uint32_t packBiomesAndLayer(BiomeId b0, BiomeId b1, uint16_t inkLayer) noexcept {
    return static_cast<uint32_t>(b0) |
           (static_cast<uint32_t>(b1) << 8u) |
           (static_cast<uint32_t>(inkLayer) << 16u);
}

/**
 * Flags for ChartVertex / ChartFieldSample.
 */
namespace ChartFlags {
    constexpr uint32_t None           = 0u;
    constexpr uint32_t IsWater        = 1u << 0u; // Ocean or lake surface
    constexpr uint32_t IsBridge       = 1u << 1u; // Timber/stone bridge crossing
    constexpr uint32_t IsSettlement   = 1u << 2u; // Town plaza cobblestone footprint
    constexpr uint32_t IsMapBanner    = 1u << 3u; // 3D world-space calligraphic map text/ribbon
    constexpr uint32_t IsDungeonMode  = 1u << 4u; // One-Page Dungeon cross-hatched interior
    constexpr uint32_t IsRhumbLine    = 1u << 5u; // Compass rose / nautical chart overlay line
} // namespace ChartFlags

/**
 * Continuous Cartographic Sample evaluated at any (worldX, worldZ) point.
 */
struct ChartFieldSample {
    float elevation{0.0f};      // Tabletop height in meters (0.0 = sea level)
    float sdfCoast{0.0f};       // Signed distance to shoreline (+ inland, - ocean)
    float sdfRiver{100.0f};     // Unsigned distance to nearest river centerline
    float riverWidth{0.0f};     // Local river half-width
    float sdfRoad{100.0f};      // Unsigned distance to nearest road/trail centerline
    float roadArc{0.0f};        // Arc-length parameter along road for dashed ink trails
    float gradX{0.0f};          // Terrain slope dH/dX for Watabou hachures
    float gradZ{0.0f};          // Terrain slope dH/dZ for Watabou hachures
    BiomeId biomePrimary{BiomeId::LushMeadow};
    BiomeId biomeSecondary{BiomeId::LushMeadow};
    float biomeBlend{0.0f};     // 0..1 blend weight for watercolor pigment pooling
    uint32_t flags{ChartFlags::None};
};

/**
 * 3D Cartographic Tabletop Vertex (64 bytes, 16-byte aligned for WGSL storage buffer pulling).
 */
struct alignas(16) ChartVertex {
    float pos[3]{0.0f, 0.0f, 0.0f};
    float sdfCoast{0.0f};       // + inland, - ocean (drives concentric water-lining rings)
    float normal[3]{0.0f, 1.0f, 0.0f};
    float sdfRoad{100.0f};      // Distance to road spline
    float gradXZ[2]{0.0f, 0.0f};// Downhill gradient (gx, gz) for topographic hachures
    float sdfRiver{100.0f};     // Distance to river centerline
    float biomeBlend{0.0f};     // Watercolor edge-darkening blend factor
    float uv[2]{0.0f, 0.0f};    // Local UV or road arc-length parameter
    uint32_t packedBiomes{0};   // low 8b = primary, next 8b = secondary, high 16b = InkLayer
    uint32_t flags{0};          // ChartFlags bitmask
};
static_assert(sizeof(ChartVertex) == 64, "ChartVertex must be 64 bytes (4x vec4) for WGSL storage alignment");

/**
 * 2.5D Inked Standee Flags.
 */
namespace StandeeFlags {
    constexpr uint32_t None           = 0u;
    constexpr uint32_t FlipX          = 1u << 0u;
    constexpr uint32_t WindSway       = 1u << 1u; // Trees, grass & banners sway gently
    constexpr uint32_t IdleBob        = 1u << 2u; // Hero, companion & monsters breathe/bob
    constexpr uint32_t Emissive       = 1u << 3u; // Campfires, runes, slash arcs
    constexpr uint32_t PartableCanopy = 1u << 4u; // Fades/parts when hero walks behind it
    constexpr uint32_t PopUpTilt      = 1u << 5u; // Tilts slightly toward tabletop camera like a standee
    constexpr uint32_t HitFlash       = 1u << 6u; // Flashes white/crimson when struck in combat
    constexpr uint32_t NoShadow       = 1u << 7u; // Skips shadow-map casting (for dense 2.5D grass tufts)
} // namespace StandeeFlags

/**
 * 2.5D Inked Standee Instance (32 bytes, 16-byte aligned for WGSL instanced rendering).
 */
struct alignas(16) StandeeInstance {
    float worldPos[3]{0.0f, 0.0f, 0.0f}; // Ground foot anchor in 3D world space
    float width{2.0f};
    float height{2.4f};
    uint32_t texLayer{InkLayer::StandeeHeroCat};
    uint32_t flags{StandeeFlags::IdleBob};
    float phaseOffset{0.0f};             // Animation phase or squash-and-stretch factor
};
static_assert(sizeof(StandeeInstance) == 32, "StandeeInstance must be 32 bytes (2x vec4) for WGSL alignment");

/**
 * Diegetic Map-Surface Combat Telegraph (32 bytes, 16-byte aligned for WGSL ground projection).
 * Renders Cat Quest style AoE circles, directional cones, and quest rings directly onto the parchment!
 */
namespace TelegraphType {
    constexpr uint32_t EnemyCircle  = 0u; // Crimson inked danger circle with expanding fill ring
    constexpr uint32_t EnemyCone    = 1u; // Directional slash/breath cone telegraph
    constexpr uint32_t PlayerArcane = 2u; // Sapphire/gold runic spell circle (Astropaw / Flamepurr)
    constexpr uint32_t QuestBeacon  = 3u; // Golden cartographic compass ring on map surface
} // namespace TelegraphType

struct alignas(16) CombatTelegraph {
    float center[3]{0.0f, 0.0f, 0.0f};
    float radius{3.5f};
    float dirX{0.0f};
    float dirZ{1.0f};
    float progress{0.5f};       // 0.0 -> 1.0 fill progress before detonation
    uint32_t telegraphType{TelegraphType::EnemyCircle};
};
static_assert(sizeof(CombatTelegraph) == 32, "CombatTelegraph must be 32 bytes (2x vec4) for WGSL alignment");

/**
 * Dynamic Point Light (32 bytes, 16-byte aligned for WGSL light pool).
 */
struct alignas(16) PointLight {
    float position[3]{0.0f, 0.0f, 0.0f};
    float radius{14.0f};
    float color[3]{1.0f, 0.72f, 0.32f};
    float intensity{1.4f};
};
static_assert(sizeof(PointLight) == 32, "PointLight must be 32 bytes (2x vec4) for WGSL alignment");

struct CameraConfig {
    Vec3f position{0.0f, 34.0f, -32.0f};
    Vec3f target{0.0f, 1.5f, 0.0f};
    float fovDegrees{32.0f};
    float focusDistance{46.0f};
    float focusRange{26.0f};
    float bokehRadius{5.0f};
    float horizonCurve{0.0018f}; // Cat Quest cylindrical tabletop scroll curvature
    float atlasZoom{0.0f};       // 0.0 = 2.5D Action Camera, 1.0 = High-Altitude Perilous Shores Atlas
};

struct AtmosphereConfig {
    Vec3f sunDirection{-0.52f, 0.64f, -0.56f};
    Vec3f sunColor{0.82f, 0.78f, 0.70f};
    Vec3f skyZenith{0.20f, 0.38f, 0.62f};   // Warm antique vellum azure zenith
    Vec3f skyHorizon{0.48f, 0.58f, 0.68f};  // Sunlit parchment horizon
    float fogDensity{0.0030f};
    float fogBaseHeight{1.0f};
    float fogHeightFalloff{0.06f};
    float exposure{0.92f};
    float timeOfDay{0.42f};
    float inkDensity{1.0f};                 // Strength of India-ink hachures & shoreline rings

    [[nodiscard]] static const char* timePeriodName(float t) noexcept {
        float frac = t - std::floor(t);
        if (frac < 0.0f) frac += 1.0f;
        if (frac < 0.20f || frac >= 0.85f) return "Starlit Midnight Watch";
        if (frac < 0.32f) return "Golden Dawn Wash";
        if (frac < 0.68f) return "Cartographer's Noon";
        return "Gilded Vellum Dusk";
    }

    [[nodiscard]] static AtmosphereConfig evaluateDayNight(float timeSecFraction) noexcept {
        float t = timeSecFraction - std::floor(timeSecFraction);
        if (t < 0.0f) t += 1.0f;

        AtmosphereConfig cfg{};
        cfg.timeOfDay = t;

        // t: 0.00 = Midnight, 0.25 = Dawn (0600), 0.50 = Noon (1200), 0.75 = Dusk (1800), 1.00 = Midnight
        float sunAngle = (t - 0.25f) * 6.2831853f; // 0 at dawn, pi/2 at noon, pi at dusk
        float sunElev = std::sin(sunAngle); // > 0 during day, < 0 at night

        // Smooth day/night curve: night is cleanly dark from 19:30 to 04:30.
        // Dawn begins at ~05:00, reaching full daylight by 07:00.
        // Sunset peaks at 18:00 (the golden sweet spot) and settles into night by 19:30.
        float dayFactor = std::clamp((sunElev + 0.15f) / 0.35f, 0.0f, 1.0f);
        dayFactor = dayFactor * dayFactor * (3.0f - 2.0f * dayFactor); // smoothstep curve

        // Dusk / Dawn golden hour glow: peaks at sunElev = 0 (06:00 and 18:00)
        float duskDawnFactor = std::clamp(1.0f - std::abs(sunElev) * 4.2f, 0.0f, 1.0f);

        // Option A: Continuous Clockwork Celestial Orbit (Astrolabe / Sundial)
        // Directional light revolves in a smooth 360-degree clockwork sweep:
        // - At 12:00 (Noon, t=0.50): Light is South (-Z), shadows cast directly North / UP (+Z).
        // - At 18:00 (Dusk, t=0.75): Light is West (-X), shadows cast hard East / RIGHT (+X).
        // - At 00:00 (Midnight, t=0.00): Light is North (+Z), shadows cast South / DOWN (-Z).
        // - At 06:00 (Dawn, t=0.25): Light is East (+X), shadows cast hard West / LEFT (-X).
        float elev = 0.30f;
        if (sunElev >= 0.0f) {
            // Day Sun: rises high in zenith at noon (0.76) for crisp short drop shadows
            elev += 0.46f * std::sin(sunAngle);
        } else {
            // Night Moon: climbs to 0.50 at midnight for clear, beautiful starlit drop shadows
            elev += 0.20f * (-std::sin(sunAngle));
        }
        float horiz = std::sqrt(std::max(0.01f, 1.0f - elev * elev));
        float clockAngle = t * 6.2831853f;
        cfg.sunDirection = {
            horiz * std::sin(clockAngle),
            elev,
            horiz * std::cos(clockAngle)
        };
        float len = std::sqrt(cfg.sunDirection.x * cfg.sunDirection.x +
                              cfg.sunDirection.y * cfg.sunDirection.y +
                              cfg.sunDirection.z * cfg.sunDirection.z);
        if (len > 1e-5f) {
            cfg.sunDirection.x /= len;
            cfg.sunDirection.y /= len;
            cfg.sunDirection.z /= len;
        }

        // Sun & Light Radiance
        // Day: warm crisp sunlight calibrated to match the optimal 18:00 gamma and rich color saturation.
        // Dusk/Dawn: golden-amber/rose. Night: near-pitch moonlight (almost total darkness).
        Vec3f daySunCol   {0.82f, 0.78f, 0.70f};
        Vec3f duskSunCol  {1.15f, 0.74f, 0.38f};
        Vec3f nightMoonCol{0.025f, 0.040f, 0.075f}; // Dim starlit moonlight
        Vec3f activeSun = {
            (1.0f - duskDawnFactor) * daySunCol.x + duskDawnFactor * duskSunCol.x,
            (1.0f - duskDawnFactor) * daySunCol.y + duskDawnFactor * duskSunCol.y,
            (1.0f - duskDawnFactor) * daySunCol.z + duskDawnFactor * duskSunCol.z
        };
        cfg.sunColor = {
            (1.0f - dayFactor) * nightMoonCol.x + dayFactor * activeSun.x,
            (1.0f - dayFactor) * nightMoonCol.y + dayFactor * activeSun.y,
            (1.0f - dayFactor) * nightMoonCol.z + dayFactor * activeSun.z
        };

        // Sky Zenith: Fresh Azure blue at noon, warm coral at sunset, pitch starlit indigo at midnight
        Vec3f dayZenith   {0.20f, 0.38f, 0.62f};
        Vec3f duskZenith  {0.36f, 0.20f, 0.44f};
        Vec3f nightZenith {0.002f, 0.003f, 0.008f};
        Vec3f activeZenith = {
            (1.0f - duskDawnFactor) * dayZenith.x + duskDawnFactor * duskZenith.x,
            (1.0f - duskDawnFactor) * dayZenith.y + duskDawnFactor * duskZenith.y,
            (1.0f - duskDawnFactor) * dayZenith.z + duskDawnFactor * duskZenith.z
        };
        cfg.skyZenith = {
            (1.0f - dayFactor) * nightZenith.x + dayFactor * activeZenith.x,
            (1.0f - dayFactor) * nightZenith.y + dayFactor * activeZenith.y,
            (1.0f - dayFactor) * nightZenith.z + dayFactor * activeZenith.z
        };

        // Sky Horizon: Warm turquoise/vellum at noon, burning peach at sunset, pitch sapphire at midnight
        Vec3f dayHorizon   {0.48f, 0.58f, 0.68f};
        Vec3f duskHorizon  {0.78f, 0.46f, 0.25f};
        Vec3f nightHorizon {0.005f, 0.008f, 0.016f};
        Vec3f activeHorizon = {
            (1.0f - duskDawnFactor) * dayHorizon.x + duskDawnFactor * duskHorizon.x,
            (1.0f - duskDawnFactor) * dayHorizon.y + duskDawnFactor * duskHorizon.y,
            (1.0f - duskDawnFactor) * dayHorizon.z + duskDawnFactor * duskHorizon.z
        };
        cfg.skyHorizon = {
            (1.0f - dayFactor) * nightHorizon.x + dayFactor * activeHorizon.x,
            (1.0f - dayFactor) * nightHorizon.y + dayFactor * activeHorizon.y,
            (1.0f - dayFactor) * nightHorizon.z + dayFactor * activeHorizon.z
        };

        cfg.fogDensity = 0.0030f + 0.0014f * (1.0f - dayFactor);
        cfg.exposure = 0.88f + 0.08f * dayFactor;
        cfg.inkDensity = 1.0f;
        return cfg;
    }
};

} // namespace eatscharted

#endif // EATSCHARTED_CHART_TYPES_HPP
