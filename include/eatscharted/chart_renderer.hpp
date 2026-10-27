#ifndef EATSCHARTED_CHART_RENDERER_HPP
#define EATSCHARTED_CHART_RENDERER_HPP

#include "eatscharted/chart_world.hpp"
#include "eatscharted/script_bindings.hpp"
#include "eatscharted/audio_foley.hpp"
#include <string>
#include <cstdint>
#include <functional>

struct GLFWwindow;

namespace eatscharted {

/**
 * Hardware-Accelerated WebGPU / Dawn 2.5D Living Cartography Renderer.
 * Implements:
 *   1. Pass 1: 2048x2048 Directional Sun Shadow Map (3D Tabletop Relief + 2.5D Paper-Cutout Standees).
 *   2. Pass 2: Living Parchment & 2.5D Standee Pass (RGBA16Float):
 *      - Concentric coastline water-lining ink rings & ocean wave dashes
 *      - Slope-aligned topographic hachures along ∇h
 *      - Watercolor biome washes with pigment edge-pooling & paper fiber grain
 *      - Spline rivers, double-cased royal highways & dashed wilderness trails
 *      - Watabou One-Page Dungeon cross-hatched stone walls & flagstone grids
 *      - Diegetic map-surface combat AoE telegraphs (circles, cones, arcane rings)
 *      - 3D curved calligraphic map banners & proximity-parting 2.5D standees
 *   3. Pass 3: Tabletop Tilt-Shift Bokeh Depth of Field + Warm Vellum Filmic Grade.
 *   4. Pass 4: Eatsbits 2D Vector Typography & HUD Overlay Pass.
 */
class ChartRenderer {
public:
    using FrameUpdateCallback = std::function<bool(GLFWwindow* window, ChartWorld& world, float dt)>;

    ChartRenderer() = default;
    ~ChartRenderer() = default;

    // Renders the ChartWorld offscreen via WebGPU and writes a 32-bit RGBA PNG file.
    static bool renderToPng(
        ChartWorld& world,
        uint32_t width,
        uint32_t height,
        const std::string& outputPngPath
    );

    // Launches the interactive desktop GLFW + WebGPU 2.5D Living Cartography Viewer & Cat Quest Controller.
    static int runInteractiveWindow(
        ChartWorld& world,
        ChartScriptEngine& scriptEngine,
        const std::string& watchedScriptPath,
        AudioFoleyEngine& foleyEngine,
        uint32_t width = 1280,
        uint32_t height = 720,
        FrameUpdateCallback frameCallback = nullptr
    );

    // Zero-dependency RGBA8 PNG encoder
    static bool writePngFile(
        const std::string& filePath,
        uint32_t width,
        uint32_t height,
        const uint8_t* rgbaPixels
    );
};

} // namespace eatscharted

#endif // EATSCHARTED_CHART_RENDERER_HPP
