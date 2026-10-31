# Encharted: Project Fork & Architectural Charter (`ENCHARTED_CHARTER.md`)

> **Document Type:** Project Genesis & Architectural Fork Specification  
> **Origin Repository:** `c:\git\eatscharted`  
> **Target Project:** **Encharted** (`c:\git\encharted` / `encharted` engine)  
> **Status:** Approved for Scaffolding & Agent Onboarding  

---

## 1. Executive Summary & Genesis of the Fork

During the architectural evolution of **Eatscharted** (documented in [`BLUEPRINT.md`](file:///c:/git/eatscharted/BLUEPRINT.md) and [`WORLD_MUTATION_BLUEPRINT.md`](file:///c:/git/eatscharted/WORLD_MUTATION_BLUEPRINT.md)), two distinct, high-potential, but mutually incompatible design directions crystallized:

1. **The Classic Macro-Cartographic Model (*Ultima 3–5 / Cat Quest / Dragon Quest*):**
   * The player traverses a macro parchment map of an entire continent where scale is abstract (cities are miniature keeps, mountain ranges are pop-up standees, oceans are crossed in seconds).
   * Entering a dungeon, castle, or town triggers an explicit transition into a dedicated sub-map operating at a discrete local scale.
2. **The Unified Seamless Open-World Model (*Ultima 6–7 / Modern Open World*):**
   * The universe exists at a single, continuous **1:1 Euclidean scale**.
   * Towns are not icons; they are sprawling districts of individual buildings, market stalls, and cobblestone plazas seamlessly laid out directly on the terrain.
   * Dungeons are not isolated portals; the player walks down a stone stairwell, the roof/surface fades away, and subterranean flagstone halls seamlessly come into view.
   * Terraforming, building construction, and world mutations are 100% physically coherent: digging a canal, breaching a wall, or tunneling into a cellar modifies the exact same world coordinate space.

Attempting to force both visions into a single codebase compromises the strengths of each. Therefore, the project is officially bifurcating into two sister engines:

```
                                  THE EATSBITS ENGINE LINEAGE
                                               │
                         ┌─────────────────────┴─────────────────────┐
                         ▼                                           ▼
                    Eatscharted                                  Encharted
         [Macro Living Cartography RPG]              [Seamless 1:1 Living World RPG]
         • Ultima 3–5 / Cat Quest paradigm           • Ultima 6–7 / Modern Open World
         • Dual-scale (Overworld + Sub-Maps)         • Single continuous 1:1 coordinate space
         • Abstract continent exploration            • Seamless towns, cutaway roofs, stairwells
         • Rapid continental voyaging & naval        • Physically coherent continuous terraforming
```

---

## 2. Comparative Paradigm Matrix

| Architectural Vector | **Eatscharted** (Current Engine) | **Encharted** (The New Fork) |
| :--- | :--- | :--- |
| **Historical Lineage** | *Ultima 3–5*, *Cat Quest*, *Dragon Quest* | *Ultima 6–7*, *Project Zomboid*, *Elder Scrolls* |
| **Spatial Coordinate System** | **Dual-Scale:** Macro atlas overworld + discrete local sub-maps at $(0, 0)$. | **Unified 1:1 Scale:** Entire world shares one continuous global coordinate space. |
| **Towns & Settlements** | Clustered `SettlementIcon` standees on the map; entering transitions to town sub-map. | Physical districts directly on the terrain: streets, plazas, individual buildings, open doors. |
| **Dungeons & Caves** | `DungeonPortal` standee wipes overworld and boots `OnePageDungeon` layout. | **Subterranean $Y$-Offset Slices:** Staircases descend into flagstone halls; roofs cut away seamlessly. |
| **Terraforming & World Mutation** | Isolated per scene file (`overworld.eats` vs. `crypt.eats`); portal boundaries hide scale mismatches. | **Physically Coherent:** Digging a moat outside a city wall touches the wall; basements connect to caves. |
| **Travel & Traversal Pacing** | Fast, high-level continental expeditions; crossing kingdoms takes 2–3 minutes. | Dense, localized world simulation; wilderness, roads, and settlements require deliberate traversal. |
| **Visual Metaphor** | Tabletop paper atlas with pop-up standees and giant calligraphic banners. | Living, hand-inked medieval world viewed from a dynamic isometric / 2.5D overhead perspective. |
| **Camera & Occlusion** | Fixed-pitch $48^\circ$ tabletop tilt with macro zoom-to-atlas. | Dynamic multi-tier camera with interior roof dissolving, stairwell aperture tracking, and height slicing. |

---

## 3. Core Architectural Pillars of Encharted

### Pillar 1: Unified 1:1 Euclidean Coordinate Space
In **Encharted**, all gameplay entities, terrain vertices, building walls, and subterranean chambers share an uncompromised global coordinate frame:
$$\mathbf{P}_{\text{world}} = (x, y, z) \in \mathbb{R}^3$$
* No teleportation between coordinate systems.
* No abstract scaling (a 1-meter flagstone is always 1 meter; a doorway is 2.2 meters high; a road is 4 to 8 meters wide).
* NPCs, projectiles, water flux, and sound propagation operate across building boundaries and cave entrances without artificial portal boundaries.

### Pillar 2: Seamless Towns, Districts & Architectural Cutaways
* **Direct Surface Placement:** Settlements are synthesized directly into the `ChartChunk` clipmap. Roads, foundations, and plazas are painted via the Voronoi / SDF field and flattened pads (`Terrain.flatten`).
* **Modular Architectural Standees & Walls:** Buildings consist of modular wall segments, doors, windows, and furnishings placed directly on flattened pads.
* **Roof Occlusion & Interior Dissolve:**
  * Each building has an inked roof cap.
  * When the hero's collision circle steps inside the building's floor footprint, the roof standee smoothly fades to $0\%$ opacity (or applies a cross-hatch stipple dither), revealing the fully decorated interior without a loading transition.

### Pillar 3: Subterranean Architecture via $Y$-Offset Slicing
To represent caves, cellars, catacombs, and deep dungeons within a 2.5D heightfield engine:
1. **Vertical Layering:**
   * Overland surface terrain sits at $y \ge 0.0\text{m}$.
   * Subterranean flagstones and crypts sit at $y \in [-15.0\text{m}, -45.0\text{m}]$ directly beneath their overland entrances.
2. **Mesh Apertures & Stairwells:**
   * Stairwell entrances, cave mouths, and cellar trapdoors cut an alpha hole or geometric aperture into the overland terrain mesh.
   * Descending a spiral stair smoothly drops the hero along the $Y$ axis.
3. **Camera Near-Plane & Height Slicing:**
   * When the camera tracks the player into subterranean depths, the camera near-plane or an interior clipping plane dissolves the surface terrain above, isolating the torchlit dungeon below.

```
Surface (Y = 0m)    ══════╗  Overland Forest / Town Plaza
                          ║
                          ║  (Spiral Stairwell Aperture)
                          ║
Subterranean (Y = -20m)   ╚═════════════════════════════ Watabou Flagstone Crypt
```

### Pillar 4: Truly Coherent World Mutation & Eatscript Persistence
Because **Encharted** operates on a single continuous coordinate space, world mutations become mathematically and physically rigorous:
* **The "TARDIS Paradox" is Eliminated:** In sub-map engines, an exterior fort measuring $20\text{m} \times 20\text{m}$ might contain an interior measuring $200\text{m} \times 200\text{m}$. In Encharted, dimensions match reality.
* **Connected Hydrology & Excavation:**
  * Players can carve a canal from an alpine lake directly into a town square fountain.
  * Players can dig an underground tunnel connecting their cellar directly to a hidden harbor cove.
* **Declarative Eatscript Spatial Registry:**
  * World diffs are indexed by a continuous 2D/3D spatial R-Tree.
  * Diffs are grouped purely by spatial chunk coordinates rather than fragmented scene identifiers.

---

## 4. Shared Lineage: What Encharted Inherits from Eatscharted

**Encharted** does not start from scratch. It directly inherits the proven, high-performance C++20 / WebGPU foundation forged in `c:\git\eatscharted`:

1. **The WebGPU 2.5D Living Cartography Pipeline:**
   * Procedural fibrous parchment substrate shader (WGSL).
   * Concentric shoreline water-lining ripples and animated wave stipples.
   * Slope-aligned topographic hachures and cross-hatching.
   * 2048×2048 directional sun shadow mapping for 2.5D standees.
2. **The Procedural Ink Synthesizer (`InkCanvas`):**
   * Procedural ink-wash tree canopies, mountain ridges, and character puppets with spring-animated bounce cycles.
3. **The Eatscript DSL & VM (`eats_script`):**
   * High-speed bytecode interpreter, AST compiler, live hot-reloading, and declarative world generation bindings.
4. **Real-time Procedural Audio (`miniaudio`):**
   * Parchment footsteps, quill/ink interface foley, sword strikes, and ambient procedural wind/water soundscapes.

---

## 5. What Encharted Evolves & Rewires

| Component | Eatscharted Implementation | Encharted Evolution |
| :--- | :--- | :--- |
| **`ChartWorld`** | Clipmap streaming tuned for macro terrain and portal triggers (`tryToggleNearbyDungeon`). | Clipmap streaming supporting multi-tier vertical coordinate offsets ($Y$-slices), aperture clipping, and seamless district streaming. |
| **Town Generation** | Placement of POI coordinates and miniature `SettlementIcon` props. | Algorithmic street routing, parcel subdivision, modular building assembly, and interior furnishing. |
| **Dungeon System** | Standalone `OnePageDungeon` generator creating an isolated 2D grid. | Embedded subterranean levels mapped directly below overland terrain with vertical stair connectors. |
| **Camera Controller** | Fixed $48^\circ$ tilt with smooth macro zoom-to-atlas. | Dynamic multi-mode camera: exterior overhead, interior room framing, and subterranean vertical tracking with automatic ceiling cutaways. |
| **Persistence ABI** | Locale-scoped `.eats` scripts keyed by scene name. | Unified spatial chunk index (`world/chunk_X_Z.eats`) containing surface and subterranean modifications in one hierarchy. |

---

## 6. Kickstart Plan for Antigravity & Gemini Agents

When launching the **Encharted** project workspace (`c:\git\encharted`):

### Immediate Milestone 1: Repository Scaffolding & Shared Core Transfer
* Clone the foundational modules (`include/eatsbits`, `include/eatscharted/ink_canvas.*`, `render/`, `audio/`, `eatscript/`) into `c:\git\encharted`.
* Rename project targets, namespaces, and build scripts to `encharted`.
* Ensure strict adherence to the **Critical Policy: No automated tests or background GPU processes** (`-NoRun -NoLaunch`).

### Immediate Milestone 2: 1:1 Scale Town District Prototype
* Implement procedural town generation on flattened terrain pads:
  * Cobblestone plaza and street splines.
  * Modular building standees (walls, doors, windows).
  * Inked roof standees with player proximity alpha-fade.

### Immediate Milestone 3: Subterranean Stairwell & Flagstone Descent
* Create the first continuous vertical transition:
  * Overland stone stairwell with mesh aperture.
  * Subterranean crypt level generated at $Y = -20.0\text{m}$.
  * Camera depth-clip shader to hide overworld terrain while underground.

---

*Charter approved for immediate project instantiation.*

