#ifndef EATSCHARTED_CARTOGRAPHIC_TEXT_HPP
#define EATSCHARTED_CARTOGRAPHIC_TEXT_HPP

#include "eatscharted/chart_types.hpp"
#include <vector>
#include <string>

namespace eatscharted {

class VoronoiAtlas;

struct PathPoint {
    Vec2f pos;      // (x, z) on ground map plane
    Vec2f tangent;  // Unit vector along text baseline
    Vec2f normal;   // Unit vector across path (pointing along letter height)
};

/**
 * Result of fitting a cartographic banner into the terrain, de-conflicting against
 * other labels and ensuring it does not fall off cliffs or cross water boundaries.
 */
struct TextFitResult {
    CurvedMapBanner banner;
    bool fitsValidly{true};
    float elevationVariance{0.0f};
};

/**
 * @brief High-performance Cat Quest & Perilous Shores Ground Path Text Renderer.
 *
 * Renders large, crisp, hand-inked cartographic serif titles directly onto the tabletop parchment
 * map plane along arbitrary curved paths or circular arcs in front of towns, dungeons, castles, and regions.
 *
 * Characteristics:
 * - Player-scale typography (height ~2.2m matching hero cat scale).
 * - Exact terrain elevation draping with hardware depth-bias (zero z-fighting).
 * - Authentic India ink lettering with soft parchment drop-shadow pass.
 * - 100% procedural vector stroke ribbons with zero texture atlas dependencies.
 */
class CartographicPathText {
public:
    /// Generates a smooth circular arc of path points in front of a landmark
    static std::vector<PathPoint> generateArcPath(Vec2f center, float radius, float startAngleRad, float endAngleRad, int samples = 48);

    /// Generates path points along a quadratic Bezier spline
    static std::vector<PathPoint> generateBezierPath(Vec2f p0, Vec2f p1, Vec2f p2, int samples = 48);

    /// Optimizes banner placement (position, orientation, span, and size) to avoid
    /// overlapping existing banners and avoid falling off steep cliffs or water edges.
    static TextFitResult fitBanner(
        const std::string& text,
        Vec2f anchorPos,
        float targetLetterSize,
        const VoronoiAtlas& atlas,
        const std::vector<CurvedMapBanner>& existingBanners,
        bool isWaterTitle = false,
        bool isRegionHeader = false
    );

    /// Emits 3D ground-draped vector stroke quads for a string along a given path
    static void emitTextAlongPath(
        const std::string& text,
        const std::vector<PathPoint>& path,
        float letterHeight,             // e.g. 2.2f meters
        const VoronoiAtlas& atlas,
        std::vector<ChartVertex>& outVerts,
        float minChunkX, float maxChunkX,
        float minChunkZ, float maxChunkZ,
        bool isWaterTitle = false
    );
};

} // namespace eatscharted

#endif // EATSCHARTED_CARTOGRAPHIC_TEXT_HPP
