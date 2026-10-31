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

## Phase 7: Core Scale Expansion (~2.4x) & Infinite Procedural Multi-Biome Continuum

- [x] **7.1 Expanded Continental Geography & Spatial Breathing Room**
  - Expanded province centers from $\sim 75\text{m}$ to $\sim 190\text{m}$:
    - **Silvervale Highlands:** `(0.0f, 190.0f)`, Highguard Keep at `(0.0f, 195.0f)`
    - **The Sunscorched Expanse:** `(190.0f, 0.0f)`, Al-Zahra Oasis at `(180.0f, -10.0f)`
    - **Kildland & Perilous Shores:** `(-190.0f, 0.0f)`, Lowmillburg Citadel at `(-175.0f, -10.0f)`
    - **The Shattered Sea:** `(0.0f, -190.0f)`, Corsair's Haven Atoll at `(0.0f, -190.0f)`
    - **The Imperial Crossroads:** Central hub at `(0.0f, 0.0f)`
  - Scaled blend standard deviations to $\sigma_{\text{prov}} = 110\text{m}$ and $\sigma_{\text{center}} = 75\text{m}$.
  - Scaled feature frequencies ($kCoordScale = 0.42$) so mountains, valleys, dunes, and bays expand smoothly across $\sim 400\text{m}$ of core terrain.
  - Extended all royal highway splines, wilderness trails, and meandering river networks to span the enlarged geography.

- [x] **7.2 Infinite Procedural Multi-Biome Continuum (Replacing the Ocean Drop-Off)**
  - Replaced the hard artificial ocean clamping (`if (rOrigin > 112m) h = -3.6m`) with an infinite procedural terrain and climate field:
    - **Macro Continental Landmass Field:** FBM tectonic noise ($C(wx, wz)$) providing balanced distribution of continental landmasses (~65%) and maritime oceans/straits (~35%).
    - **Procedural Topography:** Mountain ranges along tectonic ridges, rolling wilderness hills, and coastal shelves.
    - **Continental Climate & Moisture Gradient:** Latitudinal temperature trends and rainfall patterns driving biomes outward infinitely.
    - **Seamless $C^1$ Hermite Transition:** At $R \le 210\text{m}$, the handcrafted core continent dominates; between $210\text{m}$ and $280\text{m}$, it smoothly blends into the infinite procedural multi-biome continuum.

- [x] **7.3 Dynamic Outward Macro-Block Streaming & Discovery**
  - In `VoronoiAtlas::generateMacroBlock(bx, bz)`:
    - Automatically streams new content as the player ventures into outward chunks beyond the core continent ($|bx| > 2$ or $|bz| > 2$).
    - On land: classifies local biome, places contextual POIs (Hermitages, Monasteries, Delves, Timber Hamlets, Garrisons, Monoliths, Flour Mills), connects road links to existing highways, and arches regional banners (e.g. `"THE JADE WILDS"`, `"IRON RIDGE"`).
    - On water: places sunken ruins, lighthouses, or deep leviathan lairs in shallow seas; arches oceanic title banners across the deep abyss.
    - Dynamically detects transitions into the procedural frontier in `ChartWorld::moveHero` with new frontier banners and toasts.

---

## Phase 8: Engine Performance & Architecture Audit Optimizations

- [x] **8.1 Procedural Noise Early-Outs & Province Weight Gating (`VoronoiAtlas`)**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Early-out `hProc` when `rOrigin <= 210.0f` and early-out `hCore` when `rOrigin >= 280.0f` in `rawElevationContinental` and `rawMoistureContinental`.
  - Gate individual core province evaluations (`hHighlands`, `hDesert`, `hPeninsula`, `hArchipelago`, `hCenter`) behind `w > 1e-3f` to skip zero-weight multi-octave FBM evaluations.

- [x] **8.2 Eliminate Dead Field Evaluations (`hOff` / `mOff`) & Redundant `sampleAt` Calls**
  - Files: [`include/eatscharted/voronoi_atlas.hpp`](file:///c:/git/eatscharted/include/eatscharted/voronoi_atlas.hpp), [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp), [`src/procgen/cartographic_text.cpp`](file:///c:/git/eatscharted/src/procgen/cartographic_text.cpp)
  - Remove unused `hOff` and `mOff` evaluations from `VoronoiAtlas::sampleAt`, `VoronoiAtlas::sampleAtKnown`, and `ChartWorld::bakeChunk`.
  - Skip the unused `haloH`/`haloM` pre-pass in `ChartWorld::bakeChunk` when `voronoi_.isDungeonMode()` is active, and reuse the precomputed `samples` grid for nature standee placement.
  - In `CartographicPathText::emitTextAlongPath`, add coarse glyph-level AABB chunk culling and combine shadow + ink stroke emission into a single pass to halve `atlas.sampleAt` calls.

- [x] **8.3 Per-Frame CPU I/O, Standee Upload & Hero Sampling Optimizations**
  - Files: [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp), [`include/eatscharted/chart_world.hpp`](file:///c:/git/eatscharted/include/eatscharted/chart_world.hpp)
  - Throttle desktop script hot-reload filesystem polling (`std::filesystem::exists` / `last_write_time`) in `renderSingleFrame` to once every `0.5s`.
  - Split static and dynamic standee uploads in `uploadWorldAndDynamicEntities` so only the dynamic tail slice of `spriteBuffer` is written per frame when `!rebuildStaticWorldVerts`.
  - Eliminate redundant `voronoi_.sampleAt` calls in `ChartWorld::moveHero` when stationary and during frontier locale checks.

- [x] **8.4 Infinite World Culling, Spatial AABBs & Buffer Starvation Fixes**
  - Files: [`include/eatscharted/voronoi_atlas.hpp`](file:///c:/git/eatscharted/include/eatscharted/voronoi_atlas.hpp), [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp)
  - Change `VoronoiAtlas::generatedBlocks_` to `std::unordered_set<uint64_t>` and add precomputed AABB bounding boxes to `RiverSegment` and `RoadSegment` for fast spatial rejection in `sampleAtKnown`.
  - Distance-cull `QuestBeacon` telegraphs and prioritize combat telegraphs in `buildDynamicStandeesAndTelegraphs` so distant portals do not fill the 64-slot GPU telegraph buffer and starve local combat telegraphs.
  - Prioritize `heroTorch` in `buildDynamicStandeesAndTelegraphs` so the 64-slot point light buffer never drops the hero's light.
  - Prune dead and far-off wilderness enemies in `ChartWorld::updateSimulation` so the 24-enemy cap does not permanently lock out new enemy spawns.

- [x] **8.5 GPU Vertex Deduplication & Shader / Export Optimizations (`ChartRenderer`)**
  - Files: [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp), [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp)
  - Precompute the `33x33` unique `ChartVertex` grid corners in `ChartWorld::bakeChunk` instead of recomputing `makeVertex` 4 times per quad corner.
  - Gate overworld `macroNoise`, `mediumNoise`, and `fineGrain` FBM evaluations and Fog-of-War blending in `fs_world` so Dungeon Mode and fully charted chunks skip unused procedural math.
  - Move the 4-tap cross edge-AA filter in `fs_post` into the `else` branch of `if (coc > 0.30)` to avoid 4 redundant taps and unblurred dilution in bokeh regions.
  - Replace bit-by-bit `computeCrc32` in `ChartRenderer::writePngFile` with a `constexpr` 256-entry lookup table.

- [x] **8.6 Audio Thread Safety (`AudioFoleyEngine`)**
  - Files: [`include/eatscharted/audio_foley.hpp`](file:///c:/git/eatscharted/include/eatscharted/audio_foley.hpp), [`src/audio/audio_foley.cpp`](file:///c:/git/eatscharted/src/audio/audio_foley.cpp)
  - Synchronize voice state access between `AudioFoleyEngine::triggerSound` (main thread) and `AudioFoleyEngine::mixSamples` (audio callback thread) to eliminate C++ data races.

---

## Phase 9: Living Hydrology & Cartographic River System

- [x] **9.1 Hydrological Downhill Tracer & Logical Sources/Sinks**
  - Files: [`include/eatscharted/voronoi_atlas.hpp`](file:///c:/git/eatscharted/include/eatscharted/voronoi_atlas.hpp), [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Implemented `VoronoiAtlas::traceHydrologicalRiver(Vec2f source, float startWidth, float terminalElevation, float maxReach)`:
    - Downhill routing following continuous heightfield gradient $-\nabla h(x, z)$ with momentum/inertia smoothing.
    - Sinuosity and meander modulation based on slope steepness (straight in mountains, meandering in floodplains).
    - Leopold-Maddock hydraulic channel width scaling: $w(d) = w_0 + k \sqrt{d}$.
    - Natural ocean/sea termination ($h \le 0.02\text{m}$) with estuary delta mouth flaring.
  - Logical sources: alpine cirques / glacial melt peaks ($h > 3.5\text{m}$) and mid-elevation karst aquifer springs ($h > 1.8\text{m}$, $m > 0.62$).
  - Core continent rivers connected to drain to the open sea and supplemented with northern alpine melt tributaries.

- [x] **9.2 Spatial Grid Partitioning for O(1) Chunk River Lookups**
  - Files: [`include/eatscharted/voronoi_atlas.hpp`](file:///c:/git/eatscharted/include/eatscharted/voronoi_atlas.hpp), [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp)
  - Implemented `riverSpatialGrid_` (32m cell buckets) indexing segment bounding boxes upon creation.
  - Replaced $O(N)$ linear segment checks in `sampleAtKnown` with $O(1)$ spatial bucket lookups.
  - Increased macroblock lookahead in `ChartWorld::generateChunk` (`ensureRegionBlocks`) to 128m to guarantee upstream mountain sources are deterministically traced across chunk boundaries.

- [x] **9.3 Cartographic River Shading: Depth Fading, Shelf Inking & Directional Flow**
  - File: [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp)
  - Replaced flat river fill with multi-tiered nautical depth banding (`riverShoal` $\to$ `riverMid` $\to$ `riverDeep`) driven by normalized depth $\text{clamp}(-\text{sdfRiver} / w, 0, 1)$.
  - Inked vector shelf contour lines along internal depth boundaries mirroring oceanic shelf styling.
  - Riverbank wet silt, clay, and pebble margins darkening soil at water borders.
  - Directional downhill flow currents calculated from $-\nabla h$ (`-gradXZ`), rendering traveling current ripples, elongated foam streaks, and cartographic flow chevrons pointing downstream.
  - Downstream normal perturbation aligned with current vector.

---

## Phase 10: Web & Mobile Aspect Ratio Preservation

- [x] **10.1 Responsive 16:9 Canvas Viewport & Container Fitting**
  - File: [`web/index.html`](file:///c:/git/eatscharted/web/index.html)
  - Replaced stretching `width: 100%; height: 100%` on `canvas#canvas` with dynamic aspect-ratio constrained sizing:
    - CSS: `aspect-ratio: 16 / 9; max-width: calc(100vh * (16 / 9)); max-height: 100vh;` with `@supports` for dynamic viewport `100dvh` on mobile browsers.
    - JS: Implemented `fitCanvasAspectRatio()` calculating exact integer letterbox/pillarbox pixel dimensions fitted within `#canvas-container` without any stretching or warping.
    - Attached resize handlers to `window.resize`, `orientationchange`, and `window.visualViewport.resize`.
    - Added letterbox click-focus forwarder on `#canvas-container` so clicking outside the 16:9 canvas maintains keyboard and mouse focus.

---

## Phase 11: Dynamic Cartographic Weather & Thermodynamic Climate System

- [x] **11.1 Thermodynamic Climate Model & Elevation Lapse Rate**
  - Files: [`include/eatscharted/chart_types.hpp`](file:///c:/git/eatscharted/include/eatscharted/chart_types.hpp), [`include/eatscharted/chart_world.hpp`](file:///c:/git/eatscharted/include/eatscharted/chart_world.hpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp)
  - Implemented `WeatherType` (Clear, Overcast, Drizzle, Downpour, Thunderstorm, Flurries, Blizzard, Sandstorm) and `WeatherConfig`.
  - Implemented `ChartWorld::evaluateLocalWeather(wx, wz)`:
    - Continental province baseline thermal model: warm Sunscorched Expanse ($34^\circ\text{C}$), temperate crossroads ($18^\circ\text{C}$), cold Silvervale highlands ($4^\circ\text{C}$).
    - Orographic lapse rate: $-1.65^\circ\text{C}$ drop per meter of tabletop height, freezing mountain summits ($T \le 0^\circ\text{C}$) into snow flurries and blizzards.
    - Arid waste condition: low-moisture regions convert storm systems into ochre dust sandstorms.
  - Implemented automatic 4-minute cyclical weather front progression and dynamic wetness/snowcover accumulation.

- [x] **11.2 Cartographic Surface Shading & Weather Reactions**
  - File: [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp)
  - Extended `SceneUniforms` across C++ and all 4 WebGPU WGSL shaders with `weatherParams` (`wetness`, `snowCover`, `stormSurge`, `lightningFlash`) and `windDirection`.
  - `fs_world`:
    - Dynamic snowline descent during blizzards and cold fronts, accumulating snow on foothills and plateaus.
    - Saturated watercolor paper wetness darkening and procedural concentric ink puddle ripples on flat terrain.
    - Dynamic storm swell surge and wave chopping on ocean water.
    - Blue-white lightning strike flash illumination across terrain relief and standees.
  - `fs_sky`: Lightning illumination flashing across sky background during tempests.
  - `vs_sprite`: Wind gusts and storm surges amplifying foliage standee swaying.

- [x] **11.3 Procedural Audio Foley & Interactive Controls**
  - Files: [`include/eatscharted/audio_foley.hpp`](file:///c:/git/eatscharted/include/eatscharted/audio_foley.hpp), [`src/audio/audio_foley.cpp`](file:///c:/git/eatscharted/src/audio/audio_foley.cpp), [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp), [`src/world/script_bindings.cpp`](file:///c:/git/eatscharted/src/world/script_bindings.cpp)
  - Added procedural miniaudio synthesizers for `ThunderClap` (deep low-frequency rolling rumble), `RainPatter` (high-frequency white-noise flutter), and `WindGust` (sweeping bandpass whoosh).
  - Attached decoupling `soundCb_` in `ChartWorld` to trigger thunder rumbles on lightning strikes.
  - Added interactive `Y` key shortcut to cycle through all weather cycles on demand with discovery toasts and Foley playback.
  - Registered Eatscript DSL commands: `chart.set_weather(type, intensity)`, `chart.set_weather_cycle(enabled, speed)`, and `chart.get_weather()`.

- [x] **11.4 Camera-Frustum Bounded Instanced Weather Particles**
  - File: [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp)
  - Created `weatherParticlePipeline` integrated into Pass 2 (`scenePass`) with alpha blending and depth testing.
  - 100% GPU-evaluated particle kinematics running in vertex shader (`vs_weather_particles`):
    - Rain streaks: stretched along velocity vector $\vec{V} = (W_x, -v_y, W_z)$, rendered with India-ink needle shading.
    - Snow flakes: tumbling cut-paper hexagonal/diamond quads with harmonic sinusoidal lateral drift and camera-facing rotation.
    - Sandstorm dust: swirling ochre flecks drifting with desert gale winds.
    - 3D modulo wrapping inside a camera-pinned $48\text{m} \times 26\text{m} \times 48\text{m}$ bounding volume, giving infinite seamless coverage with zero CPU allocations or buffer uploads.

- [x] **11.5 Biome-Tethered Climate Profiles, Continuous Intensity & Sea Phase Stability**
  - Files: [`include/eatscharted/chart_types.hpp`](file:///c:/git/eatscharted/include/eatscharted/chart_types.hpp), [`include/eatscharted/chart_world.hpp`](file:///c:/git/eatscharted/include/eatscharted/chart_world.hpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp), [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp), [`src/world/script_bindings.cpp`](file:///c:/git/eatscharted/src/world/script_bindings.cpp), [`src/procgen/procedural_names.cpp`](file:///c:/git/eatscharted/src/procgen/procedural_names.cpp)
  - **Biome Climate System**: Defined `BiomeClimateProfile` with base sea-level temperatures, humidity multipliers, minimum snowline altitudes, and probabilistic weather front transition tables for every biome (meadows, taiga, alpine peaks, desert, ocean, and prepped upcoming `SnowyTundra` & `GlacialIce`).
  - **Natural Weather Front Cycling**: `updateSimulation` maps cyclical weather progression through the active biome's climate weights, naturally transitioning between sun, clouds, showers, snow flurries, and blizzards according to the local terrain.
  - **Continuous Parameter Control for "Light Snow"**:
    - Passed continuous `intensity` parameter $[0.0, 1.0]$ through `SceneUniforms`.
    - Vertex shader performs dynamic particle density culling (`seedA > intensity`), scaling active count from a few dozen flakes to full blizzard density.
    - Particle scale ($0.06\text{m} - 0.24\text{m}$) and alpha ($0.35 - 0.88$) scale smoothly with intensity, enabling fine-grained control over delicate light flurries.
    - Added `"light_snow"` DSL alias and exposed `intensity` & `precipitation` in `chart.get_weather`.
  - **Sea Wave Phase Invariance**: Fixed instantaneous wave speed-up artifact by locking wave angular phase rates constant over time and modulating amplitude + blending a high-frequency choppy wind octave via `stormSurge`.

---

## Phase 12: Icy & Snowy Terrain Generation, Glacial Shader Shading & The Frostfell Reach Scene Recipe

- [x] **12.1 Procedural Glacial & Arctic Topography Engine**
  - Files: [`include/eatscharted/voronoi_atlas.hpp`](file:///c:/git/eatscharted/include/eatscharted/voronoi_atlas.hpp), [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Added `AtlasPreset::FrostfellPeaks` to atlas preset enumerations.
  - Implemented parameterized `rawElevationFrostfell(wx, wz, center, radius)` and `rawMoistureFrostfell(wx, wz, center, radius)` designed for both standalone generation and seamless modular embedding into future overland continent expansions.
  - Sculpted distinct geological formations:
    - Soaring alpine horns and serrated arête ridges in the north/northwest using power-sharpened ridged fractal noise.
    - Broad U-shaped glacial valley and trough carved by ancestral ice flow.
    - Sunken glacial crevasse basin where thick deep-blue ice sheets pool.
    - Windblown sastrugi snowdrift undulations across the tundra plains.
    - Circular permafrost pingos (frost-heaved mounds) rising 2m above the tundra.
  - Continuous biome assignment in `sampleAt`: mapping high summits to `BiomeId::AlpinePeak`, deep depressions to `BiomeId::GlacialIce`, sheltered valley slopes to `BiomeId::TaigaPines`, and rolling permafrost plains to `BiomeId::SnowyTundra`.

- [x] **12.2 Cartographic Glacial Ice & Snowy Tundra Surface Shading**
  - File: [`src/render/chart_renderer.cpp`](file:///c:/git/eatscharted/src/render/chart_renderer.cpp)
  - Extended vertex shader (`vs_world`) with continuous `tundraFactor` (`landBiome == 13u`) and `iceFactor` (`landBiome == 14u`) interpolation passed via `terrainParams.z` and `terrainParams.w`.
  - Added dedicated fragment shader (`fs_world`) rendering passes:
    - **Glacial Ice Sheets**: Translucent azure/cyan watercolor wash, glossy specular glint catching directional sunlight, and procedural crevasse fracture chasms in crisp India ink with cerulean subsurface edge glow.
    - **Frost-Rimed Snowy Tundra**: Soft white parchment snow wash, directional wind-sculpted sastrugi wave shading, permafrost ice-wedge polygonal ground crack inking, and windswept arctic lichen/moss knolls.

- [x] **12.3 Frostfell Thematic POIs, Standees, Sled Roads & Banners**
  - Files: [`include/eatscharted/chart_world.hpp`](file:///c:/git/eatscharted/include/eatscharted/chart_world.hpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp), [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Added landmark POIs: *Frostguard Citadel*, *Rimevale Hamlet*, *Starfall Ice Spire*, *Glacial Crevasse Delve*, *Runed Ice Monolith*, and *White Wyrm's Cavern*.
  - Added glacial meltwater stream running from northwest ice horns through the U-valley into the glacial basin.
  - Synthesized curved sled highway and mountain trail splines connecting settlements and delves.
  - Placed 3D arched calligraphic map banners: `"THE FROSTFELL REACH"`, `"BLUE CREVASSE GLACIER"`, and `"RIMEVALE TUNDRA"`.
  - Added procedural biome standee populations in `bakeChunk`: frost-laden boreal pines (`StandeeForestPine`), frost crag outcrops (`StandeeRockOutcrop`), ancient rune monoliths (`StandeeStandingStone`), winter scrub patches (`StandeeGrassPatch`), and roaming frost slimes (*Frost Slime*).
  - Integrated dynamic provincial boundary detection in `moveHero` and dungeon return preset transitions in `tryToggleNearbyDungeon`.

- [x] **12.4 Eatscript DSL Bindings & Standalone Scene Recipe**
  - Files: [`src/world/script_bindings.cpp`](file:///c:/git/eatscharted/src/world/script_bindings.cpp), [`scenes/frostfell_peaks.eats`](file:///c:/git/eatscharted/scenes/frostfell_peaks.eats)
  - Registered `chart.build_frostfell_peaks(seed=...)` in the Eatscript DSL evaluator.
  - Implemented `ChartWorld::buildFrostfellPeaks(seed)` initializing atmospheric polar lighting (cool cyan horizon, crisp white sunlight) and gentle flurries weather front (`WeatherType::Flurries`).
  - Created standalone runnable scene recipe: [`scenes/frostfell_peaks.eats`](file:///c:/git/eatscharted/scenes/frostfell_peaks.eats).

---

## Phase 13: Continental Overworld Revamp (Frostfell Reach Integration & Physical Hydrology)

- [x] **13.1 Continental Six-Province Cartographic Architecture**
  - Files: [`include/eatscharted/voronoi_atlas.hpp`](file:///c:/git/eatscharted/include/eatscharted/voronoi_atlas.hpp), [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Re-architected `rawElevationContinental` and `rawMoistureContinental` with 6 Gaussian provincial centers:
    - **The Frostfell Reach (NW):** Center `(-125, 190)`, radius $170\text{m}$.
    - **The Realm of Silvervale (NE):** Center `(65, 195)`, radius $150\text{m}$.
    - **The Sunscorched Expanse (E):** Center `(190, 0)`, radius $175\text{m}$.
    - **Kildland & The Perilous Shores (W):** Center `(-185, -25)`, radius $160\text{m}$.
    - **The Shattered Archipelago (S):** Center `(0, -190)`, radius $160\text{m}$.
    - **The Imperial Crossroads (Center):** Center `(0, 0)`, radius $125\text{m}$.
  - Continuous multi-biome assignment in `sampleAt`: mapping high glacial horns and alpine ridges to `BiomeId::AlpinePeak`, deep glacial cirques to `BiomeId::GlacialIce`, sheltered evergreen slopes to `BiomeId::TaigaPines`, and permafrost plains to `BiomeId::SnowyTundra`, with frozen sea shelf ice.

- [x] **13.2 Physical Downhill Gradient Hydrological River Tracing**
  - File: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp)
  - Replaced hardcoded static Bézier splines with dynamic physical downhill gradient descent (`traceHydrologicalRiver`):
    - Traced via local descent vector $\vec{D} = -\nabla h(x, z) + \text{momentum} \cdot \vec{V} + \text{meander} \cdot \vec{N}$.
    - Adaptive sinuous meandering scaling with slope (higher sinuosity in flat plains, direct incised rushing in mountain canyons).
    - Leopold-Maddock river width progression from mountain brooks ($1.2\text{m}$) to delta estuaries ($6.5\text{m}$).
    - **5 Major River Basins**:
      1. *The Great Northern Silver River*: Source at `(55, 235)` descending through Silvervale into the Crossroads.
      2. *The Frostfell Glacial Meltwater Fjord*: Source at ice horn `(-120, 245)` cascading through the Blue Crevasse Glacier.
      3. *Western Kildland River*: Source at `(-25, 45)` winding into Lowmillburg Bay.
      4. *Southern Crossroads River & Delta*: Source at `(8, -15)` flowing into the Shattered Sea estuary.
      5. *Wadi Al-Zahra Arroyo*: Source at desert dunes `(240, 55)` cascading into the Al-Zahra Oasis basin.

- [x] **13.3 Continental POIs, Sled Highways & Grand Map Banners**
  - Files: [`src/procgen/voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp), [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp)
  - Added Frostfell landmark POIs into the continental coordinates: *Frostguard Citadel* `(-110, 190)`, *Rimevale Hamlet* `(-85, 135)`, *Glacial Crevasse Delve* `(-145, 215)`, *Starfall Ice Spire* `(-65, 225)`, *Runed Ice Monolith* `(-140, 155)`, and *White Wyrm's Cavern* `(-170, 230)`.
  - Added Imperial Northwest Highway connecting the Crossroads to Frostguard Citadel, and the Great Northern Alpine Pass linking Frostfell to Silvervale across the mountain crests.
  - Placed 3D arched calligraphic map banners including `"THE FROSTFELL REACH"`, `"THE REALM OF SILVERVALE"`, `"THE SUNSCORCHED EXPANSE"`, `"KILDLAND & THE PERILOUS SHORES"`, `"THE SHATTERED SEA"`, and `"THE IMPERIAL CROSSROADS"`.
  - Integrated Frostfell province detection and dungeon return handling in `ChartWorld`.






