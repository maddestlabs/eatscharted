#include "eatscharted/chart_world.hpp"
#include "eatscharted/script_bindings.hpp"
#include "eatscharted/audio_foley.hpp"
#include "eatscharted/chart_renderer.hpp"
#include <iostream>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    std::string scriptPath = "scenes/shattered_isles.eats";
    std::string outputPngPath;
    uint32_t width = 1280;
    uint32_t height = 720;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--render-png" || arg == "--snapshot") && i + 1 < argc) {
            outputPngPath = argv[++i];
        } else if (arg == "--script" && i + 1 < argc) {
            scriptPath = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            width = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            height = static_cast<uint32_t>(std::stoul(argv[++i]));
        }
    }

#if defined(__EMSCRIPTEN__)
    static eatscharted::ChartWorld world;
    static eatscharted::ChartScriptEngine scriptEngine(world);
#else
    eatscharted::ChartWorld world;
    eatscharted::ChartScriptEngine scriptEngine(world);
#endif

    if (std::filesystem::exists(scriptPath)) {
        std::cout << "[Eatscharted] Executing Eatscript cartography recipe: " << scriptPath << "\n";
        if (!scriptEngine.executeFile(scriptPath)) {
            std::cerr << "[Eatscharted] Eatscript error: " << scriptEngine.lastError() << "\n";
            world.buildShatteredArchipelago(1715u);
        }
    } else {
        std::cout << "[Eatscharted] Building default Shattered Isles (Corsair's Haven) 2.5D Living Cartography World...\n";
        world.buildShatteredArchipelago(1715u);
    }

    eatscharted::ChartStats stats = world.rebuildWorldGeometry();
    uint32_t totalTris = stats.mapSurfaceTriangles + stats.bannerGlyphTriangles + stats.standeeCount * 2u;
    std::cout << "[Eatscharted] 2.5D Living Cartography World compiled:\n"
              << "  - Procedural 32-Layer InkCanvas Atlas     : " << stats.atlasGenTimeMs << " ms\n"
              << "  - 3D Tabletop Parchment Relief Triangles  : " << stats.mapSurfaceTriangles << "\n"
              << "  - 3D Curved Calligraphic Banner Triangles : " << stats.bannerGlyphTriangles << "\n"
              << "  - 2.5D Paper-Cutout Standee Entities      : " << stats.standeeCount << "\n"
              << "  - Map-Surface Combat / Quest Telegraphs   : " << stats.telegraphCount << "\n"
              << "  - Total Scene Triangles                   : " << totalTris << "\n";

    if (!outputPngPath.empty()) {
        std::cout << "[Eatscharted] Rendering WebGPU 2.5D Living Cartography frame (" << width << "x" << height << ") -> " << outputPngPath << " ...\n";
        if (eatscharted::ChartRenderer::renderToPng(world, width, height, outputPngPath)) {
            std::cout << "[Eatscharted] Successfully wrote " << outputPngPath << "\n";
            return 0;
        } else {
            std::cerr << "[Eatscharted] Failed to render PNG.\n";
            return 1;
        }
    }

#if defined(__EMSCRIPTEN__)
    static eatscharted::AudioFoleyEngine foley;
#else
    eatscharted::AudioFoleyEngine foley;
#endif
    foley.init(true);
    return eatscharted::ChartRenderer::runInteractiveWindow(world, scriptEngine, scriptPath, foley, width, height);
}
