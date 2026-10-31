# Seamless Continental Overland Map Implementation Plan (`todo.md`)

> **Goal:** Stitch our four existing, high-fidelity biomes and scenes (*The Highland Kingdom*, *The Perilous Peninsula*, *The Sunscorched Wastes*, and *The Shattered Archipelago*) into a single, vast, seamless 2.5D living cartographic overland continent (**Continental Pangaea**), connected by an inter-provincial royal highway network, unified hydrology, macro calligraphic map banners, and dynamic biome-aware standee distribution.

---

## 🗺️ Master Geographic Layout: "Continental Pangaea"

```
                         NORTH: THE HIGHLAND KINGDOM
                     [Highguard Keep • Frostpeak Eyrie]
                   (Alpine Peaks, Deep Glacial Valley, Pines)
                                     ▲
                                     │ (Pass of the Whispering Wind)
                 WEST                │                EAST
        THE PERILOUS PENINSULA ◄─────┼─────► THE SUNSCORCHED WASTES
      [Lowmillburg • Stormpool]      │      [Al-Zahra Oasis • Sand Tombs]
    (Rolling Meadows, Windmills,     │     (Windblown Dunes, Sandstone Mesas,
       Oak Woods, Harbor Bay)        │          Desert Monoliths & Canyons)
                                     ▼
                      SOUTH: THE SHATTERED ARCHIPELAGO
                     [Corsair's Haven • Siren's Arch]
                   (Naval Sea Lanes, Coral Atolls, Sea Stacks)
```

---

## Phase 1: Atlas Architecture & Provincial Blending Specification

- [x] **1.1 Register Continental Preset & Province Specs**
  - File: [`include/eatscharted/voronoi_atlas.hpp`](file:///c:/git/eatscharted/include/eatscharted/voronoi_atlas.hpp)
  - Added `ContinentalPangaea` to `enum class AtlasPreset`.
  - Quadrant definitions:
    - **Highlands Province:** Center `(0.0f, 75.0f)`
    - **Peninsula Province:** Center `(-75.0f, 0.0f)`
    - **Sunscorched Province:** Center `(75.0f, 0.0f)`
    - **Archipelago Province:** Center `(0.0f, -75.0f)`
    - **Central Crossroads:** Center `(0.0f, 0.0f)`

- [x] **1.2 Analytical Multi-Province Heightfield Evaluation**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Implemented `VoronoiAtlas::rawElevationContinental(float wx, float wz)`:
    - Normalized $C^1$ radial distance-weighted partition-of-unity blending across all four provinces and the central plateau.
    - Outer continental shelf falloff dipping gracefully below $-3.6\text{m}$ for infinite open ocean sailing.

- [x] **1.3 Analytical Multi-Province Moisture & Biome Classification**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Implemented `VoronoiAtlas::rawMoistureContinental(float wx, float wz)`:
    - Highlands cool moisture ($0.72$) driving alpine crags and taiga pines.
    - Peninsula temperate moisture ($0.58$) driving deciduous oak woods and meadows.
    - Desert aridity ($0.11$) with localized Al-Zahra oasis pulse ($0.82$) driving emerald spring date palms.
    - Archipelago oceanic maritime moisture ($0.62$).

---

## Phase 2: Inter-Provincial Hydrology & The Great Imperial Highway

- [x] **2.1 Grand Continental Highway Network**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Routed double-cased ink royal highways and dashed trails:
    - **Highway 1 (North-South Spine):** Highguard Keep `(0, 78)` $\rightarrow$ Central Crossroads `(0, 0)` $\rightarrow$ Southern Cape Ferry Pier `(0, -52)`.
    - **Highway 2 (East-West Trade Route):** Lowmillburg Citadel `(-70, -4)` $\rightarrow$ Central Crossroads `(0, 0)` $\rightarrow$ Al-Zahra Caravanserai `(72, -4)`.
    - **Trail 3 (Highland Pass):** Highguard Keep $\rightarrow$ Frostpeak Eyrie $\rightarrow$ Whispering Pass $\rightarrow$ Northern Desert Garrison `(64, 32)`.
    - **Trail 4 (Peninsula Coastal & Northern Trails):** Lowmillburg $\rightarrow$ Westvale Mill $\rightarrow$ Stormpool Harbor, and Lowmillburg $\rightarrow$ Coldheartfield $\rightarrow$ Starfall Abbey.
    - **Trail 5 (Desert Trails):** Al-Zahra $\rightarrow$ Tomb of Sand Kings, and Al-Zahra $\rightarrow$ Sunwatch Minaret $\rightarrow$ The Cracked Arch.

- [x] **2.2 Continental River Basin & Shoreline Inking**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Synthesized major river splines:
    - **The Great Silver River:** Snowmelt cascade from northern peaks `(-4, 96)` through the Highland valley to the Central Crossroads `(0, 8)`.
    - **Western River Fork:** Flows from Crossroads through Kildland meadows into Lowmillburg Bay `(-84, -14)`.
    - **Southern River Fork:** Flows from Crossroads down through delta marshes into the Shattered Sea `(6, -54)`.
    - **Wadi Al-Zahra:** Dry desert arroyo carving from northeast dunes into the oasis depression.

---

## Phase 3: POIs, Landmark Standees & Hierarchical Calligraphy

- [x] **3.1 Unified Continental POI Placement**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Integrated all iconic landmarks into their respective continental world coordinates:
    - **Highland Sector:** Highguard Keep, Meadowbrook, Frostpeak Eyrie, Blackhollow Delve.
    - **Peninsula Sector:** Lowmillburg, Lowmill Grange & Westvale windmills, Coldheartfield, Stormpool Harbor & Beacon, Shattered Palace, Starfall Abbey.
    - **Desert Sector:** Al-Zahra Oasis, Tomb of Sand Kings, Dustdevil Outpost, Sunwatch Minaret, The Cracked Arch, Scorpion's Gulch.
    - **Archipelago Sector:** Corsair's Haven, Stormglass Tower, Siren's Arch, Abyssal Grotto, Leviathan Reef.
    - **Central Crossroads:** The Imperial Crossroads trading post & traveler's hub.

- [x] **3.2 Hierarchical Curved Calligraphic Map Banners**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - **Macro Continental Headers** (Scale $2.4\text{m} - 3.2\text{m}$):
    - `"THE REALM OF SILVERVALE"` (Highlands North)
    - `"THE SUNSCORCHED EXPANSE"` (Desert East)
    - `"KILDLAND & THE PERILOUS SHORES"` (Peninsula West)
    - `"THE SHATTERED SEA"` (Archipelago South, nautical water title)
    - `"THE IMPERIAL CROSSROADS"` (Center)
  - Local landmark calligraphic ribbons generated dynamically along paths in front of each POI.

---

## Phase 4: Biome-Aware Standee Ecosystem & Locale Audio/Title Toasts

- [x] **4.1 Dynamic Standee Adaptation in Chunk Baking**
  - File: [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp) (`bakeChunk`)
  - Continuous classification drives procedural standee distribution across chunks:
    - Taiga pines and crag outcrops in the northern highlands.
    - Oak forest canopies, lush grass, and windmills across the western peninsula.
    - Date palms, sandstone monoliths, and dune scrub across the eastern desert.
    - Basalt sea stacks, coral reefs, and caravel water traversal in the southern sea.

- [x] **4.2 Provincial Boundary Detection & Discovery Toasts**
  - File: [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp) (`moveHero`)
  - Added continuous provincial boundary tracking:
    - Seamlessly detects transitions between *The Realm of Silvervale*, *The Sunscorched Expanse*, *Kildland*, *The Shattered Sea*, and *The Imperial Crossroads*.
    - Automatically triggers `triggerLocaleDiscovery(provName, provSub)` with animated UI toast ribbons.

---

## Phase 5: Engine API, Script Bindings & Master Scene Recipe

- [x] **5.1 ChartWorld C++ Core Method**
  - File: [`include/eatscharted/chart_world.hpp`](file:///c:/git/eatscharted/include/eatscharted/chart_world.hpp) & [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp)
  - Implemented `void ChartWorld::buildContinentalPangaea(uint32_t seed = 2026u);`
  - Spawns hero at the Imperial Crossroads `(0.0f, 1.75f, 0.0f)`.
  - Added `ContinentalPangaea` handling in dungeon exit transitions (`tryToggleNearbyDungeon`).

- [x] **5.2 Eatscript DSL Member Binding**
  - File: [`src/world/script_bindings.cpp`](file:///c:/git/eatscharted/src/world/script_bindings.cpp)
  - Registered `chart.build_continental_overworld(seed=...)`.

- [x] **5.3 Master Scene Script**
  - File: [`scenes/continental_overworld.eats`](file:///c:/git/eatscharted/scenes/continental_overworld.eats)
  - Created runnable master scene recipe uniting all biomes.

---

## Phase 6: Build & Verification Checklist (Strict Manual User Testing)

> ⚠️ **CRITICAL POLICY:** Do NOT run automated tests (`eatscharted_tests.exe`, `ctest`, etc.) or launch background GPU processes. All verification is performed manually by the user.

- [x] **6.1 Compilation Verification**
  - Compiled successfully via `build.ps1 -NoRun -NoLaunch`:
    - Zero compiler errors, zero linker warnings.
    - Binary verified: `build/Release/eatscharted.exe`.

- [ ] **6.2 User Manual Experience Test Items (For User Manual Testing)**
  - [ ] **Hero Spawn:** Run `.\build\Release\eatscharted.exe --script scenes/continental_overworld.eats` (or launch via debugger/IDE). Hero spawns at the Imperial Crossroads `(0, 0)` with the central banner.
  - [ ] **Northbound Journey:** Walk north along Highway 1 $\rightarrow$ observe ascent into the Highland Kingdom; taiga pines and crag outcrops appear; Highguard Keep looms over the river valley.
  - [ ] **Eastbound Journey:** Walk east along Highway 2 $\rightarrow$ cross the mountain rain-shadow into the Sunscorched Wastes; sand dunes, sandstone buttes, and Al-Zahra Oasis date palms emerge.
  - [ ] **Westbound Journey:** Walk west along Highway 2 $\rightarrow$ enter Kildland; rolling green pastures, windmills, and Lowmillburg Castle appear.
  - [ ] **Southbound Journey:** Walk south along Highway 1 $\rightarrow$ reach the southern cape; step into the waters of the Shattered Sea to board the ship and sail among Corsair Atoll and sea stacks.
  - [ ] **Calligraphy Banners & Region Toasts:** Grand banners curve above each province; toast ribbons announce discovery when entering each territory.
