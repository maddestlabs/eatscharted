# Eatscharted Architectural Blueprint (`BLUEPRINT.md`)

> **Vision:** A procedural 2.5D "Living Cartography" action-RPG and world-exploration engine built on C++20, WebGPU (`wgpu-native` / Google Dawn / Emscripten), `miniaudio` procedural foley, and the Eatscript DSL. **Eatscharted** fuses the antique pen-and-ink procedural cartography of **Watabou's *Perilous Shores*** (Voronoi dual-meshes, concentric coastline water-lining, topographic hachures, clumped ink-wash forests, spline roads, and curved calligraphic labels) with the tactile 2.5D tabletop map traversal and real-time combat of ***Cat Quest***.

---

## 1. Executive Summary & Lineage

**Eatscharted** is the third pillar in the Eatsbits engine family:
1. **[Eatsbits](file:///c:/git/eatsbits/engine-blueprint.md):** The foundational C++20 / WebGPU engine providing `eats_script` (Eatscript VM, compiler, live hot-reload, AOT transpiler), `BatchRenderer2D`, vector glyph/SVG path tessellation, and real-time `miniaudio` synthesis.
2. **[Eatsrealms](file:///c:/git/eatsrealms/README.md):** The Octopath-style HD-2D engine introducing multi-pass WebGPU shadow-mapped rendering, instanced 2.5D normal-mapped billboards, tilt-shift bokeh post-processing, sliding clipmap chunk streaming, and *The Archaeology of Intent* narrative history simulation.
3. **Eatscharted (`c:\git\eatscharted`):** Replaces stepped voxel blocks with a **continuous, procedurally inked 2.5D parchment atlas** where players walk, dodge-roll, cast spells on, and sail across a living map populated by 2.5D paper-cutout standees, curved map banners, and seamless *One-Page Dungeon* sub-maps.

```
┌────────────────────────────────────────────────────────────────────────────┐
│                        Eatscript Cartography DSL (.eats)                   │
│  • World Seeds, Tectonic Masks, Biome Palettes, Ink & Parchment Styles     │
│  • Chronicle History Rules, Quest Hooks, Combat Spell Telegraphs           │
└─────────────────────────────────────┬──────────────────────────────────────┘
                                      │ Host ABI
┌─────────────────────────────────────▼──────────────────────────────────────┐
│                   Layer 1: Dual-Mesh Cartographic Generator                │
│  ┌──────────────────────────────┐   ┌────────────────────────────────────┐ │
│  │ Poisson + Lloyd Voronoi Mesh │   │ Graph Hydrology & Road Network     │ │
│  │ • Coastline fractalperturber │   │ • Voronoi-edge river trees & lakes │ │
│  │ • Orographic rain shadows    │   │ • Delaunay A* trade roads & bridges│ │
│  │ • Biome & Region Classifier  │   │ • MacroHistoryGrid POI placement   │ │
│  └──────────────┬───────────────┘   └─────────────────┬──────────────────┘ │
│                 └───────────────────┬─────────────────┘                    │
│                                     ▼                                      │
│  ┌───────────────────────────────────────────────────────────────────────┐ │
│  │           Streaming ChartChunk Rasterizer (Clipmap Ring)              │ │
│  │ • 64×64 SDF & Cartographic Sample Grid per Chunk (48m × 48m)          │ │
│  │ • d_coast (Coastline SDF), d_river, d_road, Elevation h, Gradient ∇h  │ │
│  │ • 2.5D Pop-Up Standee Placement & Curved Banner Spline Extraction     │ │
│  └──────────────────────────────────┬────────────────────────────────────┘ │
└─────────────────────────────────────┼──────────────────────────────────────┘
                                      │ WebGPU Storage Buffers & SDF Textures
┌─────────────────────────────────────▼──────────────────────────────────────┐
│              Layer 2: Dawn / WebGPU 2.5D Cartographic Pipeline             │
│  • Pass 1: 2048×2048 Directional Sun Shadow Map (2.5D Paper-Cutout Cast)   │
│  • Pass 2: Living Parchment Ground Pass (WGSL)                             │
│      - Procedural paper fiber grain + watercolor pigment edge-pooling      │
│      - Concentric ink shoreline ripples & animated wave stippling          │
│      - Slope-aligned topographic hachures & cross-hatching along ∇h        │
│      - Dashed ink roads, river ribbons, & diegetic combat AoE telegraphs   │
│  • Pass 3: 2.5D Inked Standees & Curved Calligraphic Map Banners           │
│      - Multi-layer mountain ridge cutouts, clumped ink-wash tree canopies  │
│      - Spring-animated 2.5D hero/enemy puppets & parting foliage occlusion │
│      - Spline-warped vector typography flat on the map surface             │
│  • Pass 4: Tabletop Horizon Curve, Tilt-Shift Bokeh & Filmic Grade         │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Architectural Pillars

### Pillar 1: Dual-Mesh Cartographic World Generation (*Perilous Shores* Layer)
Unlike grid-bound heightmap generators that betray their underlying square tile grid, Watabou's *Perilous Shores* achieves organic realism by synthesizing worlds on an **irregular polygonal dual graph** before projecting to chunks:

1. **Blue-Noise Poisson-Disc Sampling & Lloyd Relaxation:**
   - Generate $N$ seed points across a regional domain using fast bridged Poisson-disc sampling, followed by 2 iterations of Lloyd relaxation to produce well-proportioned **Voronoi polygons** (regions/cells) and their dual **Delaunay triangulation**.
2. **Tectonic Elevation & Fractal Coastlines:**
   - Assign elevation to Voronoi vertices via plate/ridge splines and multi-octave Simplex noise.
   - Subdivide coastline Voronoi edges using **midpoint displacement with tangential jitter** so peninsulas, coves, barrier islands, and sea stacks exhibit authentic hand-drawn jaggedness.
3. **Graph Hydrology (Rivers, Estuaries & Lakes):**
   - Water flows strictly downhill along **Voronoi cell edges** (corners) from high mountain ridges to the sea (`elevation <= 0`), accumulating flux ($Q$).
   - High-flux edges widen into meandering rivers, estuaries, or inland freshwater lakes.
4. **Prevailing Winds & Biome Classification:**
   - Advect moisture from ocean cells along prevailing wind vectors; mountain ridges cast **orographic rain shadows**, naturally placing lush forests on windward slopes and arid steppes/badlands on leeward plains.
   - Biomes map directly to cartographic ink/wash treatments:
     - `OceanDeep`, `CoastalShoals`, `SandyBeach`, `SaltMarsh`, `LushMeadow`, `DeciduousWoodland`, `TaigaPines`, `AridSteppe`, `BadlandsCanyon`, `AlpinePeak`, `CalamityBlight`.
5. **Civilization, Roads & The Archaeology of Intent:**
   - Extend Eatsrealms' `MacroHistoryGrid` onto the Voronoi graph: harbor towns spawn at river mouths, citadel keeps on high choke-points, monasteries on alpine ridges, and sunken ruins in marshes.
   - Connect settlements via **A\* pathfinding over the Delaunay graph** (weighted by slope and river crossings, automatically spawning inked stone bridges where roads cross rivers).

---

### Pillar 2: Streaming `ChartChunk` & Multi-Channel Cartographic SDF
To allow infinite / multi-kilometer exploration at 120+ FPS while keeping shader evaluation fast, the vector graph is rasterized on-demand into a sliding clipmap of **`ChartChunk`** instances ($48\text{m} \times 48\text{m}$ world space, backed by a $64 \times 64$ cartographic field):

```cpp
namespace eatscharted {

/**
 * Per-sample cartographic field packed into a 64x64 chunk texture / storage buffer.
 * Interpolated bilinearly in the WGSL ground shader for infinite vector crispness.
 */
struct ChartFieldSample {
    float elevation;        // Smooth tabletop height (0.0 = sea level, >0 = land, <0 = sea)
    float sdfCoast;         // Signed distance to shoreline (+ inland, - ocean, in meters)
    float sdfRiver;         // Unsigned distance to nearest river spline (with flux width)
    float sdfRoad;          // Unsigned distance to nearest road/trail spline
    uint8_t biomePrimary;   // Primary BiomeId
    uint8_t biomeSecondary; // Neighbor BiomeId for watercolor edge bleeding
    uint8_t biomeBlend;     // 0..255 blend factor + pigment edge-darkening weight
    uint8_t flags;          // Bridge, ford, ruin footprint, cobblestone plaza, etc.
};

} // namespace eatscharted
```

**Why this hybrid Vector-Graph $\to$ Chunk-SDF approach wins:**
- **Zero Polygon Aliasing:** The WGSL fragment shader evaluates smooth analytical `fwidth()` anti-aliased ink strokes from the interpolated SDF values (`sdfCoast`, `sdfRiver`, `sdfRoad`), staying razor-sharp at any camera zoom level.
- **Deterministic Streaming:** Chunks evaluate purely from the regional Voronoi cache in $< 1.5\text{ms}$ on background worker threads, reusing WebGPU storage buffers with zero runtime allocations.

---

### Pillar 3: The *Perilous Shores* WGSL Cartographic Shader Pipeline
The ground surface in **Eatscharted** is a gently undulating 3D tabletop mesh (with subtle relief for hills and sunken riverbeds) shaded like a master cartographer's ink-and-watercolor manuscript:

1. **Parchment Substrate & Pigment Pooling:**
   - Multi-scale procedural fibrous paper texture (`fbm` + domain-warped grain) modulates both surface albedo and micro-normal roughness.
   - Biome transitions apply **wet-on-dry watercolor edge darkening** (`smoothstep` ring at `biomeBlend` boundaries) so meadows, marshes, and deserts look hand-tinted.
2. **Concentric Shoreline Ink Hatching (Water-Lining):**
   - For ocean fragments (`sdfCoast < 0.0`), compute periodic distance waves:
     ```wgsl
     let shoreDist = -sdfCoast;
     let ripplePhase = shoreDist * kRippleFreq - sin(time * 1.5 + worldPos.x * 0.4) * 0.15;
     let ringMask = smoothstep(0.45, 0.55, abs(fract(ripplePhase) - 0.5))
                  * exp(-shoreDist * kShoreFalloff);
     ```
   - Produces 3–5 crisp, hand-drawn concentric ink rings hugging every island, cove, and sandbar, transitioning into rhythmic horizontal wave dashes in deep water.
3. **Topographic Slope Hachures:**
   - Compute terrain slope magnitude $\|\nabla h\|$ and downhill direction $\hat{g} = \text{normalize}(\nabla h)$.
   - Project world coordinates onto the contour-tangent and slope-parallel axes to draw **Watabou-style hachure ticks**—short, tapered pen strokes whose density and line weight scale with slope steepness and shadow orientation.
4. **Spline Roads, Dotted Trails & Marshes:**
   - `sdfRoad` renders double-inked casing lines for royal highways, or **dashed pen strokes** (`fract(roadArcLength * kDashFreq)`) for wilderness trails.
   - Marsh and desert biomes procedurally stamp horizontal grass-tuft ticks and sand stipple clusters in world space.

---

### Pillar 4: 2.5D "Pop-Up Cartography" Standees & Curved Map Typography
To achieve the ***Cat Quest*** look where the world feels simultaneously like a flat map and a living 3D toy-box:

```cpp
enum class StandeeCategory : uint8_t {
    MountainRidge,      // Multi-layered inked ridge cutout with hachure shading
    ForestCanopy,       // Clumped ink-wash tree canopy puffs + trunk under-shadow
    SettlementIcon,     // 2.5D miniature inked buildings (keep, hamlet, windmill, lighthouse)
    DungeonPortal,      // Archway / cave maw / sunken ruin entrance with pulsing rune glow
    LandmarkProp,       // Standing stones, shipwrecks, signposts, campfires, treasure chests
    CharacterPuppet,    // Hero, companions, monsters, naval vessel (with squash-and-stretch)
    MapBannerGlyph      // World-space calligraphic label glyph quad along a curved spline
};
```

1. **Procedural Inked Standee Atlas (`InkCanvas`):**
   - Replaces Eatsrealms' pixel-art `PixelCanvas` with **`InkCanvas`**: a procedural higher-resolution ($128 \times 128$ to $256 \times 256$ per layer) vector-ink rasterizer that synthesizes variable-width India-ink outlines, interior hachure shading, watercolor washes, and normal maps into a WebGPU `Texture2DArray`.
2. **Clumped Forest Massing:**
   - Rather than scattering thousands of noisy individual trees, woods are grouped into **linked canopy clusters** with clean outer ink silhouettes and stippled interior foliage, parting or fading to $35\%$ opacity when the player walks behind them.
3. **Diegetic Curved Calligraphic Map Labels:**
   - Region names (*"THE PERILOUS SHORES"*, *"ASHENFELL REACH"*), settlement banners, and sea titles are laid out along **quadratic Bézier curves** derived from mountain spines or coastlines.
   - Uses Eatsbits' vector font engine (`vector_font.hpp`) to emit instanced world-space quads hovering $0.05\text{m}$ above the parchment, casting subtle drop shadows onto the map.

---

### Pillar 5: *Cat Quest* Tabletop Camera, Kinematics & Overworld Action Combat

1. **Tabletop Camera & Optional Horizon Roll:**
   - Fixed-pitch overhead camera ($\sim 48^\circ$ pitch) with smooth spring-damped player tracking.
   - **Cylindrical World Curvature (Vertex Shader):** Optional toggle applying `worldPos.y -= kHorizonCurve * max(0.0, camDistZ - kFlatRadius)^2`, rolling distant map territory gently over a tabletop horizon while keeping the foreground flat and distortion-free.
   - **Seamless Zoom-to-Atlas:** Scrolling out smoothly transitions from the $48^\circ$ 2.5D action camera to a $85^\circ$ macro cartographic atlas view, scaling up regional map banners and quest markers.
2. **Continuous 2D Circle-vs-SDF Kinematics & Naval Transitions:**
   - Smooth 360° movement velocity, directional dodge-roll with invulnerability frames and ink-puff particles, and bounce/squash walk-cycle animation on 2.5D character standees.
   - **Land $\leftrightarrow$ Sea State Machine:** Collision queries `sdfCoast` directly. Stepping onto a pier or acquiring a Harbor Charter lets the player seamlessly hop into a 2.5D inked caravel/sloop when crossing `sdfCoast < 0`, unlocking coastal island hopping and sea-monster encounters.
3. **Diegetic Map-Surface Combat Telegraphs:**
   - Enemy attacks and player spells render **inked/arcane telegraph decals directly on the parchment shader** (radial fill circles, directional cones, linear charge tracks) before triggering 2.5D slash ribbons, hit-stop, and floating calligraphic damage numbers.
4. **Watabou *One-Page Dungeon* Sub-Maps:**
   - Stepping into a `DungeonPortal` standee seamlessly cross-fades the tabletop into an interior **One-Page Dungeon map**—complete with cross-hatched stone wall perimeters, flagstone grids, rubble, braziers, and boss rooms rendered by the exact same 2.5D cartographic pipeline.

---

## 3. Repository & Module Structure (`c:\git\eatscharted`)

```
c:\git\eatscharted\
├── BLUEPRINT.md                           # This architectural blueprint
├── README.md                              # Project overview & quickstart
├── CMakeLists.txt                         # C++20 + Dawn/WebGPU + Emscripten build
├── build.ps1                              # Windows build script (-NoRun -NoLaunch safe)
├── build-web.ps1                          # Emscripten WebGPU build script
├── include/
│   ├── eatsbits/                          # Shared Eatscript VM, ABI & Vector Typography
│   └── eatscharted/
│       ├── chart_types.hpp                # Vec2/3, BiomeId, ChartFieldSample, StandeeInstance
│       ├── voronoi_atlas.hpp              # Poisson-disc, Voronoi/Delaunay graph, hydrology, roads
│       ├── chart_chunk.hpp                # 64x64 SDF/field chunk & standee/banner buffers
│       ├── chart_world.hpp                # Clipmap streaming world, SDF collision, combat decals
│       ├── ink_canvas.hpp                 # Procedural pen-and-ink standee & icon synthesizer
│       ├── map_label_layout.hpp           # Curved Bézier spline calligraphic label generator
│       ├── one_page_dungeon.hpp           # Watabou One-Page Dungeon interior generator
│       ├── player_controller.hpp          # Cat Quest 2.5D movement, dodge-roll, combat & sailing
│       ├── chart_renderer.hpp             # Dawn/WebGPU 4-pass 2.5D cartographic renderer
│       ├── audio_foley.hpp                # Parchment footsteps, quill UI, sword/spell/surf foley
│       └── script_bindings.hpp            # Eatscript .eats world & quest bindings
├── src/
│   ├── eatscript/                         # Eatscript compiler, VM, host ABI
│   ├── procgen/
│   │   ├── voronoi_atlas.cpp              # Perilous Shores macro generator & history sim
│   │   ├── ink_canvas.cpp                 # Procedural inked mountains, trees, towns, heroes
│   │   ├── map_label_layout.cpp           # Curved map text & ribbon banners
│   │   └── one_page_dungeon.cpp           # Procedural hatched dungeon interiors
│   ├── world/
│   │   ├── chart_chunk.cpp                # Vector-to-SDF chunk rasterizer
│   │   ├── chart_world.cpp                # Streaming clipmap, entities, combat telegraphs
│   │   ├── player_controller.cpp          # Kinematics, combat state machine, land/sea transition
│   │   └── script_bindings.cpp            # Eatscript API registration
│   ├── render/
│   │   └── chart_renderer.cpp             # WGSL parchment, shoreline ripples, hachures, standees
│   ├── audio/
│   │   └── audio_foley.cpp                # Procedural miniaudio soundscapes
│   ├── ui/                                # Vector font & 2D HUD batch renderer
│   └── main.cpp                           # Interactive desktop/web app & headless PNG snapshotter
├── scenes/
│   ├── perilous_shores.eats               # Default flagship coastal peninsula & archipelago
│   ├── shattered_isles.eats               # Naval exploration & sea-stack archipelago
│   ├── highland_kingdom.eats              # River valley, royal roads, citadel & ruins
│   └── sunken_sanctum_dungeon.eats        # One-Page Dungeon hatched interior showcase
└── tests/
    └── chart_core_tests.cpp               # Headless unit tests for Voronoi, SDF, and kinematics
```

---

## 4. Phased Implementation Roadmap

### Phase 1: Foundation, Voronoi Cartographer & Chunk SDF Rasterizer
- [ ] Port the shared `eats_script`, `miniaudio`, `glfw`, `wgpu-native`, and `ui/glyph` foundation from `c:\git\eatsrealms` into `c:\git\eatscharted`.
- [ ] Implement `VoronoiAtlas` (`voronoi_atlas.hpp/.cpp`): Poisson-disc sampling, Voronoi/Delaunay topology, fractal coastline subdivision, downhill river trees, rain-shadow biomes, and settlement road routing.
- [ ] Implement `ChartChunk` (`chart_chunk.hpp/.cpp`): rasterizing `ChartFieldSample` (`elevation`, `sdfCoast`, `sdfRiver`, `sdfRoad`, `biomePrimary/Secondary`) across a streaming clipmap grid.

### Phase 2: The *Perilous Shores* WGSL Living Parchment Renderer
- [ ] Implement `ChartRenderer` (`chart_renderer.hpp/.cpp`) with 3-pass WebGPU rendering (Shadow Map $\to$ HDR Cartographic Scene $\to$ Tilt-Shift & Filmic Post-Process).
- [ ] Author the WGSL Living Parchment ground shader: fibrous paper grain, watercolor biome edge-pooling, concentric shoreline water-lining rings, animated ocean stippling, slope-aligned topographic hachures, and dashed/cased road & river splines.

### Phase 3: `InkCanvas` 2.5D Pop-Up Standees & Curved Map Typography
- [ ] Build `InkCanvas` (`ink_canvas.hpp/.cpp`) to procedurally synthesize hand-inked 2.5D standee sprites: layered hachured mountain peaks, clumped ink-wash tree canopies, miniature town keeps/hamlets/windmills, dungeon arches, and hero/monster puppets.
- [ ] Build `MapLabelLayout` (`map_label_layout.hpp/.cpp`) to place curved calligraphic region titles and town banners directly onto the map surface along coastlines and mountain ridges.
- [ ] Add proximity parting/transparency so tall forest canopies and mountain ridges smoothly fade when the player passes behind them.

### Phase 4: *Cat Quest* Tabletop Controller, Combat Telegraphs & Sailing
- [ ] Implement `PlayerController` (`player_controller.hpp/.cpp`): responsive 2.5D movement, squash-and-stretch walk cycle, directional dodge-roll, and smooth SDF shoreline sliding.
- [ ] Add seamless **Land $\leftrightarrow$ Sea Sailing** when crossing harbors/beaches (`sdfCoast < 0`), morphing the player into an inked 2.5D sailing vessel with wake ripples.
- [ ] Implement **Diegetic Map-Surface Combat**: ground-projected ink/arcane AoE telegraph circles/cones, melee slash arcs, enemy AI behaviors, and bouncing damage numbers.
- [ ] Add **Zoom-to-Atlas** camera transitions and optional cylindrical tabletop horizon curvature.

### Phase 5: Watabou *One-Page Dungeon* Interiors & Eatscript Quest Chronicle
- [ ] Implement `OnePageDungeonGenerator` (`one_page_dungeon.hpp/.cpp`) with cross-hatched wall perimeters, flagstone grids, secret doors, and boss arenas triggered when entering overworld dungeon portals.
- [ ] Connect `MacroHistoryGrid` lore generation to interactive overworld quest markers, lore obelisks, and live-reloadable `.eats` cartography recipes.
