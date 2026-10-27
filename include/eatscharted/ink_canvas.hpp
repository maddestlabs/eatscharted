#ifndef EATSCHARTED_INK_CANVAS_HPP
#define EATSCHARTED_INK_CANVAS_HPP

#include "eatscharted/chart_types.hpp"
#include <vector>
#include <cstdint>

namespace eatscharted {

/**
 * Holds the complete 32-layer 64x64 RGBA8 Albedo+Alpha and Normal+Emissive Texture2DArray data
 * for Eatscharted's hand-inked 2.5D cartographic standees and parchment surfaces.
 */
struct InkAtlasData {
    uint32_t width{InkLayer::kTileSize};
    uint32_t height{InkLayer::kTileSize};
    uint32_t layers{InkLayer::kCount};
    std::vector<uint8_t> albedoBytes; // width * height * layers * 4
    std::vector<uint8_t> normalBytes; // width * height * layers * 4 (RG=normal XY, B=normal Z, A=emissive)
};

/**
 * Procedural Pen-and-Ink Standee & Cartographic Icon Synthesizer.
 * Generates Watabou-style hachured mountain peaks, clumped ink-wash tree canopies,
 * miniature settlement icons, and Cat Quest 2.5D hero/ship/monster standees.
 */
class InkCanvas {
public:
    static InkAtlasData generateFullAtlas(uint32_t seed = 1492u);
};

} // namespace eatscharted

#endif // EATSCHARTED_INK_CANVAS_HPP
