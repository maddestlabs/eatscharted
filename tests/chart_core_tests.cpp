#include "eatscharted/chart_world.hpp"
#include "eatscharted/script_bindings.hpp"
#include "eatscharted/audio_foley.hpp"
#include <cassert>
#include <iostream>

int main() {
    eatscharted::ChartWorld world;
    world.buildPerilousPeninsula(1492u);
    auto stats = world.rebuildWorldGeometry();

    assert(stats.mapSurfaceTriangles > 10000);
    assert(stats.bannerGlyphTriangles > 100);
    assert(stats.standeeCount > 10);

    // Test Cat Quest land <-> sea sailing transition
    world.moveHero(-20.0f, -20.0f, false, 1.0f);
    world.triggerMeleeSlash();
    world.triggerArcaneSpell();

    // Test Watabou One-Page Dungeon builder
    world.buildOnePageDungeon(999u);
    auto dStats = world.rebuildWorldGeometry();
    assert(dStats.mapSurfaceTriangles > 10000);
    assert(world.voronoi().isDungeonMode());

    std::cout << "[eatscharted_tests] All headless unit assertions passed.\n";
    return 0;
}
