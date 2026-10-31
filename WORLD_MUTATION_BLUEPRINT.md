# Eatscharted World Mutation & Eatscript Persistence Blueprint (`WORLD_MUTATION_BLUEPRINT.md`)

> **Vision:** A groundbreaking persistence and world-manipulation architecture where player-driven world modifications (terraforming, architectural construction, dungeon reclamation, and population extinction) are compiled, stored, and executed as declarative **Eatscript (.eats)** code diffs over an analytical procedural cartography base.

---

## 1. Engine Reality Check: Voxels vs. Cartographic Heightfields

### 1.1 Has Eatscharted Implemented Voxels?
**No, Eatscharted does not use voxels.** 
As established in [`BLUEPRINT.md`](file:///c:/git/eatscharted/BLUEPRINT.md) and [`src/world/chart_world.cpp`](file:///c:/git/eatscharted/src/world/chart_world.cpp):
* The overworld terrain is a **continuous 2.5D triangulated cartographic heightfield mesh** ($H(x, z)$) driven by domain-warped fractal Brownian motion (FBM) and Voronoi graph hydrology.
* Props, vegetation, structures, enemies, and characters are rendered as **2.5D paper-cutout standees** (`StandeeInstance`) on the parchment surface.
* Dungeons and interior structures are handled via the **Watabou *One-Page Dungeon*** model (`DungeonFlagstone`, `DungeonHatchedWall`, and room bounds).

### 1.2 Do We Still Have Deep Terraforming Control Without Voxels?
**Yes—and for an antique living cartography aesthetic, heightfields are actually superior to voxels in speed, memory, and visual elegance:**

| Terraforming Goal | How It Works on Heightfield + Standees | Level of Control |
| :--- | :--- | :--- |
| **Castle Plateau / Courtyard** | Flatten disc or polygon to target height $H_0$ with smooth edge falloff | **Total & Instantaneous** |
| **Earthen Ramparts & Hills** | Add Gaussian or spline-profile ridge offset $\Delta H(x, z)$ | **Total & Instantaneous** |
| **Moats, Trenches & Canals** | Subtract trench profile, lower below water level to turn into flowing water | **Total & Instantaneous** |
| **Terraced Mountain Farming** | Step-quantize elevation along contour lines | **Total & Instantaneous** |
| **Architecture (Cottages, Castles)** | Place discrete architectural standees/prefabs on flattened terrain | **Total & Fully Modular** |
| **Caves & Underground Cellars** | *Hybrid Portal / Volume Approach* (Cut hole in surface + enter sub-map/crypt) | **Seamless** |

A pure heightfield cannot do a 3D hollow overhang *within the single continuous surface sheet*, but for 95% of world interaction (flattening mountains, building walls, digging moats, carving roads), heightfield manipulation is mathematically exact, consumes almost zero memory, and never suffers from voxel "staircasing" or heavy polygon meshing.

---

## 2. The Architectural Paradigm: Semantic Modular Construction vs. Voxel Micro-Digging

### 2.1 Why Voxel Micro-Digging Is the Wrong Fit
Micro-voxel digging (chipping away cube by cube with a pickaxe) has major drawbacks:
1. **Player Tedium:** Clearing space for a castle or dungeon requires thousands of repetitive clicks.
2. **Visual Roughness:** Without complex artistic auto-tiling, micro-voxel excavations look jagged, noisy, and clash with the refined, hand-inked aesthetic of Watabou and antique cartography.
3. **Save File & AST Bloat:** As discussed in Hurdle 1, recording thousands of tiny coordinate pings creates enormous ASTs and requires complex compaction passes.

### 2.2 The Solution: "Townscaper / One-Page Dungeon" Semantic Chunking
Instead of micro-digging, *Eatscharted* adopts **semantic architectural modules and macro-brushes** (reminiscent of the legendary *Far Cry Map Maker*, *Townscaper*, and *Dungeon Keeper*):

* **Overworld Terraforming (Far Cry Style):**
  * Players paint intent using high-level brushes: **Level Foundation** (creates a flat pad with natural retaining slope), **Moat/Trench** (depresses land and fills with water), or **Terrace Step**.
* **Castle & Home Construction:**
  * Players place cohesive architectural components:
    * `GrandHall` (with timber rafters and stained glass)
    * `Watchtower` (with spiral stair and crenellations)
    * `CorridorWithWindows` vs. `VaultedPassageway`
    * `CourtyardPerimeter` (parapet walls with corner turrets)
* **Dungeon & Cave Redesign (Watabou One-Page Dungeon Style):**
  * Rather than mining rock atom by atom, players place **Rooms & Passages**:
    * `Dungeon.excavateRoom({ center: [x, z], size: [w, d], style: "crypt" })`
    * `Dungeon.carveCorridor({ from: [x1, z1], to: [x2, z2], width: 3.0, arched: true })`
    * `Dungeon.sealPassage({ at: [x, z] })`

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Player Creative Action                             │
│       "I want an underground banquet hall connecting to my crypt"       │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
                 ┌───────────────────┴───────────────────┐
                 ▼                                       ▼
    [The Voxel / Pickaxe Way]              [The Eatscharted Semantic Way]
  • Click 4,800 individual blocks.       • Click "Excavate Banquet Chamber (14x10)"
  • 4,800 voxel state diffs.             • Click "Connect Arched Corridor"
  • Jagged, uneven walls.                • Emits 2 clean, elegant Eatscript calls:
  • Massive AST / binary bloat.            Dungeon.excavateRoom(...);
                                           Dungeon.carveCorridor(...);
                                         • Instantly renders with Watabou-style
                                           cross-hatched ink perimeter & flagstones.
```

---

## 3. Is "World as Eatscript Code" Experimental?

**Yes, it is highly experimental in the commercial game industry.**

* **The Industry Status Quo:** Games almost universally persist changes as **opaque binary blobs** (e.g., SQLite databases in *No Man's Sky*, custom binary chunk files in *Minecraft* and *Valheim*, or flatbuffers/protobufs).
* **The Eatsbits / Eatscharted Paradigm:** Treating the universe as an **executable codebase** where player interactions are **declarative Eatscript source diffs** merges concepts from:
  1. **Constructive Solid Geometry (CSG)** and CAD (like *OpenSCAD*).
  2. **Homoiconic virtual worlds** (like *Smalltalk-76/80* and *LambdaMOO*).
  3. **Event sourcing without simulation fragility** (logging declarative mathematical outcomes rather than fragile physics inputs).

Because it is experimental, we must anticipate the real-world engineering hurdles before laying code.

---

## 4. The 4 Big Architectural Hurdles & Their Solutions

```
┌────────────────────────────────────────────────────────────────────────┐
│                   Player Performs In-Game Action                       │
│  (Pickaxe flatten, wall placement, dungeon purge, moat excavation)     │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Emits high-level intent
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                  Eatscript Code Synthesis & Compaction                 │
│  • Emits declarative Eatscript API call (e.g. Terrain.flatten(...))    │
│  • Compaction Pass: merges adjacent cuts / removes redundant calls     │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
          ┌─────────────────────────┴──────────────────────────┐
          ▼                                                    ▼
┌──────────────────────────────────┐ ┌──────────────────────────────────┐
│        Persistence Layer         │ │     In-Engine Evaluation         │
│  • Human-readable .eats scripts  │ │  • Spatial AABB R-Tree indexing  │
│  • Git-style diffs & branches    │ │  • SIMD / GPU terrain evaluation │
│  • Modular per-locale files      │ │  • Direct scene-graph mutation   │
└──────────────────────────────────┘ └──────────────────────────────────┘
```

### Hurdle 1: The "10,000 Pickaxe Swings" Problem (AST Bloat)
* **The Risk:** If a player chips a cliff 5,000 times, storing 5,000 `Terrain.carve()` function calls creates an enormous AST that slows down parsing and runtime evaluation.
* **The Solution: Semantic Tooling & Compaction Passes**
  1. *Tool Design:* Player terraforming tools operate on geometric volumes (e.g., 4m flatten pads, 2m trench plows, rectangular foundation stakes) rather than single-pixel micro-scratches.
  2. *AST Compactor (Optimizer Pass):* When saving or idling, the engine analyzes adjacent operations within the chunk:
     * Overlapping subtractive spheres coalesce into an oriented capsule or box.
     * Flattening operations sharing a common elevation merge into a polygon convex hull.

### Hurdle 2: Chunk Generation Performance
* **The Risk:** If every chunk has to run a JavaScript/Eatscript VM interpreter to compute elevation for 33×33 vertices, frame rates will tank.
* **The Solution: Static Parameter Extraction into C++ POD Structs**
  * When an Eatscript locale loads, the script is evaluated **once** to populate a C++ `SpatialModificationRegistry`.
  * Operations are extracted into flat Plain-Old-Data (POD) structs:
    ```cpp
    struct HeightModifier {
        enum class Type { Flatten, Trench, Ridge, Terrace };
        Type type;
        AABB bounds;
        Vec2f center;
        float radius;
        float param0, param1;
    };
    ```
  * During chunk mesh generation in `ChartWorld::generateChunkMesh()`, the vertex shader or CPU loop simply tests the vertex coordinate against the active modifiers intersecting that chunk's AABB.

### Hurdle 3: Ordering & Commutativity (CSG Stack Collapse)
* **The Risk:** In CSG, order matters ($A - B \ne B - A$). If diffs get reordered during patch merges, the landscape could look mangled.
* **The Solution: Layered Operational Buckets**
  Operations are classified into deterministic execution stages:
  1. **Base Terrain:** $H_{\text{base}} = \text{VoronoiAtlas}(x, z)$
  2. **Coarse Earthwork (Additive/Subtractive):** Trenches, moats, hills ($\pm \Delta H$).
  3. **Flattening / Terracing:** Clamping to target plane (Courtyards, foundations).
  4. **Architectural Graph:** Placed standees, walls, roofs, doors.
  5. **Site Metadata:** Faction claims, spawner suppression zones.

### Hurdle 4: Scale (The Whole Universe Cannot Be One Script)
* **The Risk:** A single `world.eats` file for an entire continent or planet will become unmanageable.
* **The Solution: Hierarchical Locale Scoping**
  The universe is organized into a directory structure matching the Voronoi / MacroHistory grid:
  ```
  save_world/
  ├── realm_manifest.eats          // World seed, active calendar, global factions
  ├── settlements/
  │   └── port_pawven.eats         // Harbor modifications, trade status
  └── locales/
      ├── chunk_04_12/
      │   └── mountain_cottage.eats// Player's mountain home
      └── dungeons/
          └── crypt_of_skulls.eats // Cleared dungeon claimed as habitat
  ```
  Untouched wilderness locales generate **0 files** on disk. Only inhabited or modified locales exist.

---

## 5. The Proposed Eatscript World API Specification

### 4.1 Module: `eatscharted/terrain`
Declarative procedures modifying the 2.5D cartographic heightfield:

```javascript
import { Terrain } from "eatscharted";

// 1. Flatten an area for building a mountain cottage or castle
Terrain.flatten({
    center: [142.0, -85.5],
    radius: 18.0,
    targetElevation: 6.2,
    blendDistance: 4.0
});

// 2. Excavate a moat around the courtyard
Terrain.carveTrench({
    path: [
        [120.0, -80.0],
        [160.0, -80.0],
        [160.0, -100.0],
        [120.0, -100.0],
        [120.0, -80.0]
    ],
    width: 3.5,
    depth: 2.0,
    fillWater: true // Lowers below water table, creates water-lining ink ripples
});

// 3. Raise an earthen rampart along a ridge
Terrain.raiseRidge({
    along: [[115.0, -75.0], [165.0, -75.0]],
    height: 3.2,
    width: 4.0
});
```

### 5.2 Module: `eatscharted/dungeon` (One-Page Dungeon Macro-Carving)
Excavating and remodeling underground crypts, corridors, and living quarters:

```javascript
import { Dungeon, InkLayer } from "eatscharted";

// 1. Carve a grand vaulted banquet hall out of the mountain interior
Dungeon.excavateRoom({
    id: "mountain_great_hall",
    center: [240.0, 180.0],
    dimensions: [20.0, 14.0], // 20m x 14m hall
    floorStyle: "flagstone_royal",
    wallHatching: "dense_granite",
    features: [
        { type: "hearth_double", at: [240.0, 173.2] },
        { type: "chandeliers_iron", count: 3 }
    ]
});

// 2. Connect the Great Hall to the natural spring cave via an arched corridor
Dungeon.carveCorridor({
    from: [240.0, 187.0],
    to: [240.0, 215.0],
    width: 3.5,
    style: "arched_passage",
    hasWindows: false,
    torchesEvery: 6.0
});

// 3. Wall off and seal an infested orc burrow
Dungeon.sealPassage({
    at: [228.0, 180.0],
    blockType: "mortared_stone_barricade"
});
```

### 5.3 Module: `eatscharted/architecture`
Placing modular room and corridor units on the overworld (courtyards, watchtowers, cottages):

```javascript
import { Architecture } from "eatscharted";

// Building a mountain cliffside redoubt
Architecture.placeModule({
    type: "WatchtowerOctagonal",
    at: [142.0, 8.5, -90.0],
    features: ["crenellations", "arrow_slits", "beacon_fire"]
});

Architecture.placeModule({
    type: "PassagewayWithWindows",
    from: [142.0, 8.5, -90.0],
    to: [156.0, 8.5, -90.0],
    roof: "timber_slate_hip"
});
```

### 5.4 Module: `eatscharted/settlement`
Decimating villages, clearing dungeons, and setting spawner suppression:

```javascript
import { World, Settlement } from "eatscharted";

// Decimate an Orc Village
Settlement.modify({
    settlementId: "orc_camp_bloodtooth",
    status: "ruins",
    population: 0,
    banners: InkLayer.BannerTatteredRuins,
    spawnersActive: false
});

// Inhabit and claim a cleared dungeon as a player lair
World.claimLocale({
    localeId: "dungeon_crypt_04",
    name: "Under-Mountain Haven",
    claimedBy: "player",
    suppressMonsterSpawns: true,
    safeRestZone: true
});
```

---

## 6. Implementation Roadmap for Eatscharted

```
Phase 1: Heightfield Modification Overlay in ChartWorld (C++ Engine)
├── Add HeightModifier list and evaluation to ChartWorld::generateChunkMesh()
└── Implement flatten, trench, and ridge mathematical kernels

Phase 2: Eatscript Host ABI Binding
├── Bind Terrain.flatten(), Terrain.carveTrench() to host registry (src/world/script_bindings.cpp)
└── Test round-trip script execution populating the C++ modifier pipeline

Phase 3: Prefab / Architectural Standee Placement
├── Support dynamic placement of custom standees with persistence
└── Link StandeeInstance lifecycle to Eatscript Architecture.placeStructure()

Phase 4: AST Serialization & Locale Diff Saver
├── Implement in-game tool interactions (flatten brush, build tool)
├── Auto-generate Eatscript statements from committed player actions
└── Save to locale-scoped .eats files upon chunk unmount or manual save
```

---

## 7. Summary

* **Voxels are not needed** to achieve breathtaking terraforming in *Eatscharted*. Continuous heightfield elevation modifications + 2.5D architectural standees give full freedom to flatten mountain crags, build castles, dig moats, and sculpt habitats.
* **The Eatscript approach is truly innovative**: it elevates save files into readable, editable, moddable open-source modules, giving the player the feeling that they are actively scripting the history and geography of their world.

