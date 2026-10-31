# Continental Procedural Generation Blueprint (`CONTINENTAL_PROCGEN_BLUEPRINT.md`)

> **Vision:** A vast, coherent, and living fantasy overland world in the spirit of the legendary *Ultima* lineage (from the macro-cartographic exploration and naval voyaging of *Ultima 3–5* to the simulated world coherence and systemic depth of *Ultima 6–7*). The entire continent is deterministically derived from a **Single Master Seed**, combining our modular regional procgen recipes into a seamless, breathtaking overland journey where players traverse from snowy alpine peaks down through river valleys and lush forests, across sunscorched deserts, and into blissful coastal archipelagoes.

---

## 1. Core Philosophy & Design Pillars

### 1.1 The *Ultima* Spirit: An Open, Living World with Our Own Identity
While inspired by Richard Garriott's pioneering works, our world is **not Britannia**. We establish our own distinct mythos, geographic provinces, and antique visual identity:
* **The High-Seas & Overland Freedom (*Ultima 3–5*):** True non-linear exploration. Players can chart their own course across rugged cordilleras, follow meandering riverways on foot, or board a caravel to sail through archipelago straits and barrier reefs.
* **World Coherence & Purposeful Ecology (*Ultima 6–7*):** Every settlement, road, windmill, dungeon entrance, and bandit redoubt exists for a geographic or societal reason. A mountain fortress guards the snowmelt pass; a lumber hamlet nestles in the dense pine valley; a bustling port occupies the natural sheltered harbor.
* **Handcrafted in Feel, Procedural in Engine:** The player should never feel like they are wandering through random mathematical noise. The procedural generator acts as an invisible master cartographer, placing features with aesthetic intent, historical resonance, and rhythmic gameplay pacing.

### 1.2 The "One True Seed" Deterministic Hierarchy
A single 32-bit or 64-bit integer seed (`world_seed`) deterministically cascades through all physical, ecological, and cultural systems:

$$\text{Seed} \longrightarrow \text{Tectonics \& Continentalness} \longrightarrow \text{Climate / Hydrology} \longrightarrow \text{Macro-Provinces} \longrightarrow \text{Infrastructure} \longrightarrow \text{Lore \& Encounters}$$

* **100% Reproducible:** Sharing a world seed produces the exact same mountain passes, river deltas, secret grottos, and city banners.
* **Extensible via Eatscript Diffs:** Player actions, terraforming, and story progressions do not mutate raw static meshes; they are recorded as declarative Eatscript (`.eats`) delta operations layered atop the immutable seed (as specified in [`WORLD_MUTATION_BLUEPRINT.md`](file:///c:/git/eatscharted/WORLD_MUTATION_BLUEPRINT.md)).

---

## 2. Continental Biome Spectrum & Traversal Journey

The continental geography is structured around a natural, evocative physical progression:

```
[ NORTH / HIGH ELEVATION ]
             ▲
             │   1. Silvervale Highlands (Snowy Peaks, Glacial Cirques, Mountain Keeps)
             │      • Derived from: highland_kingdom.eats
             │      • Inked crags, conifer tree standees, winding alpine passes
             │
             │   2. The Verdant Midlands (Temperate River Valleys & Ancient Forests)
             │      • Clumped ink-wash deciduous canopies, rolling green hills
             │      • Watermills, royal highway bridges, farming hamlets
             │
             │   3. Sunscorched Basin / Rain-Shadow Wastes (Arid Deserts & Mesas)
             │      • Orographic barrier created by the northern cordillera
             │      • Sand dune ripples, cracked earth hachures, sun-bleached ruins
             │
             │   4. The Perilous Shores & Coral Archipelago (Bays, Ports & Reefs)
             │      • Derived from: perilous_shores.eats & shattered_isles.eats
             │      • Concentric ink water-lining, barrier islands, sheltered anchorages
             ▼
[ SOUTH-WEST / SEA LEVEL ]
```

---

## 3. Four-Tier Procedural Generation Architecture

To compose our individual procgen scenes into a sprawling, contiguous overland map without seams or unnatural transitions, the generation pipeline operates across four strictly ordered tiers:

```
┌────────────────────────────────────────────────────────────────────────┐
│ TIER 1: Global Continental Framework                                   │
│ • Continentalness SDF: Landmass boundaries vs. Ocean                   │
│ • Continental Elevation Gradient: Alpine Spine → Coastal Shelf        │
│ • Whittaker Climate Model: Global Temperature & Moisture Fields        │
└───────────────────────────────────┬────────────────────────────────────┘
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ TIER 2: Voronoi Macro-Province Partitioning & Domain Blending         │
│ • Lloyd-relaxed macro Voronoi cells defining regional boundaries       │
│ • Biome archetype recipes mapped to province centers (cx, cz)          │
│ • Continuous heightfield weight-blending: H(x, z) = ∑ w_i · H_i        │
└───────────────────────────────────┬────────────────────────────────────┘
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ TIER 3: Continental Connective Tissue (Hydrology & Roads)              │
│ • Orographic river networks: Snowmelt springs → Valleys → Coast       │
│ • Delaunay/A* arterial road splines: Royal highways & mountain trails   │
│ • Bridges at river crossings & lighthouses on dangerous headlands      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ TIER 4: Cartographic Dressing, Settlements & Locale Discovery         │
│ • 2.5D Standee flora/props (Biome-specific density & distribution)    │
│ • Calligraphic banner placement along curved Bezier splines            │
│ • Settlement POIs & Watabou One-Page Dungeon portal integration        │
└────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Tier 1: Global Continental Framework

1. **Continentalness Field $C(x, z) \in [-1, 1]$:**
   * Uses low-frequency Simplex / Fractal Brownian Motion (FBM) with large domain warping:
     $$C(x, z) = \text{FBM}\left(\frac{x + W_x(x, z)}{\lambda_{\text{continent}}}, \frac{z + W_z(x, z)}{\lambda_{\text{continent}}}\right)$$
   * $C > 0$ denotes dry land; $C \le 0$ defines the ocean, triggering animated shoreline ripples and water-lining.
2. **Continental Elevation Profile $E(x, z)$:**
   * A macro-gradient placing tectonic uplift in the north/north-east, sloping downward to sea level in the south-west.
3. **Whittaker Climate Matrix (Temperature $T$ and Moisture $M$):**
   * **Temperature $T(x, z)$:** Driven by latitude (warmer south, colder north) minus adiabatic lapse rate based on elevation ($\Delta T \propto -H$).
   * **Moisture $M(x, z)$:** Windward ocean breezes hit the mountain ranges; precipitation falls on the windward slopes (dense forests), creating an **orographic rain shadow** on the leeward interior (arid deserts).

### 3.2 Tier 2: Voronoi Macro-Province Partitioning & Domain Blending

Rather than hardcoding origin-centered recipes, regional generators (such as [`highland_kingdom.eats`](file:///c:/git/eatscharted/scenes/highland_kingdom.eats) and [`perilous_shores.eats`](file:///c:/git/eatscharted/scenes/perilous_shores.eats)) are decoupled into **Province Archetypes** assigned to macro Voronoi cells:

1. **Province Seed Points:**
   A set of $K$ regional seed points $\mathbf{P}_k = (x_k, z_k)$ distributed across the continent via Poisson disk sampling and relaxed with Lloyd's algorithm.
2. **Archetype Assignment:**
   Each cell evaluates its average $(E, T, M)$ to select its regional recipe:
   * High $E$, low $T$: **Highland Kingdom** (alpine ridges, glacial rivers, stone keeps).
   * Medium $E$, high $M$: **Verdant Midlands** (rolling hills, deciduous woodlands, windmills).
   * Low $E$, low $M$: **Sunscorched Wastes** (dunes, canyon gullies, sandstone ruins).
   * Near Coast ($C \approx 0$): **Perilous Shores** (fractal peninsulas, barrier bays, port towns) or **Shattered Archipelago** (isolated isles, sea stacks).
3. **Smooth Boundary Blending:**
   To prevent cliff-like seams between provinces, the elevation $H(x, z)$ at any world coordinate is evaluated as a smooth normalized blend of adjacent province height functions:
   $$H(x, z) = \sum_{k \in \mathcal{N}} w_k(x, z) \cdot H_k(x - x_k, z - z_k)$$
   Where weights $w_k$ drop off smoothly using a cubic Hermite falloff based on the distance to province Voronoi borders.

### 3.3 Tier 3: Continental Connective Tissue (Hydrology & Trade Routes)

The connective tissue is what elevates isolated procgen zones into a believable, interconnected continent:

1. **Hydrological River Graph:**
   * Springs spawn in high-elevation highland cells.
   * Water flows along the steepest descent gradient $\nabla H$, joining tributaries into wider rivers.
   * Meandering splines carve natural riverbeds through the midlands, bypass the desert plateaus, and discharge into sheltered coastal bays as deltas.
2. **Continental Road & Trade Network:**
   * A minimum spanning tree (MST) augmented with Delaunay triangulation edges connects major settlement POIs.
   * Path costs penalize steep slopes and deep water, naturally guiding roads through mountain passes and river fords.
   * Road segments render as double-cased ink lines through civilized regions, transitioning into dashed trails in wilderness and mountain passes.

### 3.4 Tier 4: Cartographic Dressing & World Discovery

1. **2.5D Standee Distribution:**
   * Props are placed via deterministic Poisson dart throwing keyed to local moisture and terrain slopes:
     * Highlands: Snow-capped pines, granite boulders, watchtowers.
     * Midlands: Oak clusters, apple orchards, timber-frame cottages, windmills.
     * Deserts: Saguaro cacti, sandstone monoliths, nomad tents.
     * Shores: Palm fronds, fishing shacks, lighthouses, beached shipwrecks.
2. **Curved Calligraphic Region Banners:**
   * Evaluated along Bezier splines bounding geographic features:
     * Province titles: `"THE SILVERVALE MARCHES"`, `"KILDLAND REALM"`, `"THE SHATTERED SEA"`.
     * Landmark titles: `"HIGHGUARD KEEP"`, `"LOWMILLBURG"`, `"STORMGLASS LIGHT"`.
3. **Locale Discovery Banners:**
   * As the hero traverses province borders or approaches POIs, the camera and UI trigger dynamic parchment discovery banners (`ChartWorld::triggerLocaleDiscovery`), mirroring the joy of cartographic discovery in classic RPGs.

---

## 4. Scene Scripting & Eatscript DSL Integration

Our existing standalone `.eats` scenes remain first-class citizens, now functioning as focused biome workbenches alongside the continental master script:

### 4.1 Standalone Biome Workbenches (Testing & Iteration)
* [`scenes/highland_kingdom.eats`](file:///c:/git/eatscharted/scenes/highland_kingdom.eats): Fast iteration on alpine heightfields, snowmelt rivers, and mountain keeps.
* [`scenes/perilous_shores.eats`](file:///c:/git/eatscharted/scenes/perilous_shores.eats): Iteration on peninsular bays, water-lining, and port settlements.
* [`scenes/shattered_isles.eats`](file:///c:/git/eatscharted/scenes/shattered_isles.eats): Iteration on naval navigation, reefs, and caravel sailing.
* [`scenes/sunken_sanctum_dungeon.eats`](file:///c:/git/eatscharted/scenes/sunken_sanctum_dungeon.eats): Watabou *One-Page Dungeon* subterranean sub-maps.

### 4.2 The Continental Master Recipe (`scenes/overland_continent.eats`)
```python
# Eatscharted Grand Overland Map: The Known Realm
chart.clear()

# Initialize the global continental framework from the Master Seed
chart.build_continental_world(
    seed=1492,
    dimensions=[512.0, 512.0],  # 512m x 512m macro overland canvas
    sea_level=0.0,
    mountain_spine_elevation=12.0
)

# Place the Hero high in the northern fortress
chart.set_hero_pos(x=12.0, z=180.0)
chart.trigger_discovery("HIGHGUARD PASS", "Silvervale Realm")
```

---

## 5. Architectural Implementation Roadmap

| Milestone | Objective | Key Deliverables |
| :--- | :--- | :--- |
| **Phase 1: Coordinate Decoupling** | Parameterize existing preset generators | Update `VoronoiAtlas` in [`voronoi_atlas.cpp`](file:///c:/git/eatscharted/src/procgen/voronoi_atlas.cpp) so height, moisture, and POI equations accept an arbitrary origin `(cx, cz)` and orientation rather than relying on `(0, 0)`. |
| **Phase 2: Macro Climate & Province Graph** | Implement Tier 1 & Tier 2 generation | Add `ContinentalGrid` evaluating macro elevation $E(x, z)$, temperature $T(x, z)$, and moisture $M(x, z)$. Construct the Voronoi province graph with smooth boundary blending. |
| **Phase 3: Connective Hydrology & Arterial Roads** | Unify cross-biome features | Implement continuous gradient-descent river carving and the cross-province trade road spline generator. |
| **Phase 4: Continental Streaming & Memory** | Infinite chunk streaming across continent | Extend `ChartChunk` streaming ring to seamlessly page chunk heightfield SDFs, standees, and banners as the hero travels from peaks to sea. |
| **Phase 5: Sub-Map Dungeon & Town Integration** | Connective portals & interiors | Seamlessly link overworld dungeon entrances (`ABYSSAL GROTTO`, `SUNKEN SANCTUM`) to Watabou *One-Page Dungeon* sub-maps using the dual-scale model defined in [`ENCHARTED_CHARTER.md`](file:///c:/git/eatscharted/ENCHARTED_CHARTER.md). |

