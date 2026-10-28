#ifndef EATSCHARTED_PROCEDURAL_NAMES_HPP
#define EATSCHARTED_PROCEDURAL_NAMES_HPP

#include "eatscharted/chart_types.hpp"
#include "eatscharted/voronoi_atlas.hpp"
#include <string>
#include <string_view>
#include <cstdint>

namespace eatscharted {

/**
 * Procedural Name & Spline Banner Generator.
 * Synthesizes antique fantasy cartography names (e.g. Duskwoods, Dragon Desert,
 * Lowmillburg, Coldheartfield, Stormpool, Shattered Palace, Fort of the Undead-One)
 * and lays them out along 3D Bézier splines directly across the living map surface.
 */
class ProceduralNameGenerator {
public:
    // Generates a settlement, dungeon, or landmark name conditioned on seed, biome, and category
    [[nodiscard]] static std::string generateLocaleName(uint32_t seed, BiomeId biome, PoiCategory category);

    // Generates a major geographic region or realm title (e.g., Kildland, Duskwoods, Dragon Desert)
    [[nodiscard]] static std::string generateRegionTitle(uint32_t seed, BiomeId biome);

    // Formats text with Cat Quest antique letter tracking ("Duskwoods" -> "D u s k w o o d s")
    [[nodiscard]] static std::string formatCatQuestTracking(std::string_view text, bool doubleSpace = false);

    // Generates a quadratic Bézier spline (p0, p1, p2) curved along a geographic direction
    [[nodiscard]] static CurvedMapBanner createCurvedBanner(
        const std::string& text,
        Vec2f center,
        float spanWidth,
        float angleRadians,
        float archCurvature, // positive = upward convex arch, negative = concave dip
        float letterSize = 2.4f,
        bool isRegionHeader = true,
        bool isWaterTitle = false
    );
};

} // namespace eatscharted

#endif // EATSCHARTED_PROCEDURAL_NAMES_HPP
