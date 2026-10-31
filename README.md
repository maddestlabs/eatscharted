# Eatscharted (`c:\git\eatscharted`)

**Eatscharted** is a native C++20 / WebGPU 2.5D "Living Cartography" action-RPG and world-exploration engine built on the [Eatsbits](file:///c:/git/eatsbits/engine-blueprint.md) and [Eatsrealms](file:///c:/git/eatsrealms/README.md) foundation.

See it live: [Eatscharted on GitHub Pages](https://maddestlabs.github.io/eatscharted/)

It synthesizes:
- **Watabou's *Perilous Shores* Procedural Cartography:** Voronoi/Delaunay dual-mesh world generation, fractal coastlines with concentric ink water-lining rings, topographic slope hachures aligned with $\nabla h$, clumped ink-wash forests, cased royal highways & dashed wilderness trails, watercolor biome washes with pigment edge-pooling, and 3D curved calligraphic map banners.
- ***Cat Quest*'s 2.5D Tabletop Map Gameplay:** High-angle 2.5D exploration directly across an undulating parchment world map populated by shadow-casting 2.5D paper-cutout standees, continuous SDF collision, seamless land-to-sea caravel sailing, ground-projected combat AoE telegraphs, zoom-to-atlas camera transitions, and Watabou *One-Page Dungeon* cross-hatched interiors.
- **Diegetic Cartographic Fog-of-War & Ink Bloom:** Uncharted wilderness appears as raw antique vellum with faint graphite pencil contours; as the hero travels, an organic capillary ink-bleeding front washes over the map, blooming into vibrant watercolors and bold calligraphy.
- **Seamless Chunk-Based World Streaming:** Fixed-coordinate $32\text{m} \times 32\text{m}$ chunk grid with O(1) cached memory, eliminating stutters and scenery popping for infinite navigation.

## Building & Running

### Desktop (Windows / WebGPU Native)

```powershell
# Build and launch interactive 2.5D Living Cartography viewer
.\build.ps1

# Compile only without launching
.\build.ps1 -NoRun -NoLaunch

# Render headless PNG preview snapshot
.\build.ps1 -RenderPreview
```

### Web & Mobile (WebAssembly + WebGPU)

```powershell
# Compile to WebAssembly + WebGPU and launch local dev server (0.0.0.0:8080)
.\build-web.ps1

# Compile WebAssembly + WebGPU bundle only without launching server
.\build-web.ps1 -NoServe

# Serve on a custom port
.\build-web.ps1 -Port 9000
```

## Controls (*Cat Quest* Tabletop & Cartographic Atlas)

- **`W / A / S / D` or Arrow Keys:** Move Hero across land, or sail the inked Caravel across bays and shoals
- **`Left Shift`:** *Cat Quest* Dodge-Roll / Sprint
- **`Space` / `J` / `Left Click`:** Melee Sword Slash (cleaves nearby enemies with knockback & floating damage numbers)
- **`K` / `Right Click`:** Cast Arcane Nova (projects a glowing runic AoE circle onto the parchment map)
- **`E` / `Enter`:** Enter / Exit nearby **Watabou *One-Page Dungeon*** portal (`SUNKEN SANCTUM`)
- **`T`:** Toggle dynamic **Day/Night Cycle** (smooth orbital sun & moon light, shifting atmospheric watches)
- **`[` / `]`:** Step Time of Day backward / forward by 1 hour (adjust sun angle and hatch drop shadows)
- **`M` / `Z`:** Toggle **Zoom-to-Atlas** mode (smoothly transitions between the 2.5D action camera and a high-altitude *Perilous Shores* cartographic map view)
- **`1 / 2 / 3 / 4 / 5`:** Switch Chart Recipe:
  - `1`: `scenes/shattered_isles.eats` (*The Shattered Isles* - Corsair's Haven & Naval Archipelago)
  - `2`: `scenes/perilous_shores.eats` (*The Perilous Shores* Peninsula, Bay & Barrier Islands)
  - `3`: `scenes/highland_kingdom.eats` (*Silvervale Realm* Twin-Ridge River Valley)
  - `4`: `scenes/sunken_sanctum_dungeon.eats` (*The Sunken Sanctum* Cross-Hatched One-Page Dungeon)
  - `5`: `scenes/sunscorched_wastes.eats` (*The Sunscorched Wastes* - Arid Dunes, Terraced Mesas & Oasis)

## Documentation

- **[Architectural Blueprint (`BLUEPRINT.md`)](file:///c:/git/eatscharted/BLUEPRINT.md):** Full technical specification, WGSL shader design, data structures, and module layout.
- **[Continental Procedural Generation Blueprint (`CONTINENTAL_PROCGEN_BLUEPRINT.md`)](file:///c:/git/eatscharted/CONTINENTAL_PROCGEN_BLUEPRINT.md):** Continental overland design, single-seed deterministic hierarchy, macro-climate Whittaker modeling, Voronoi province blending, and connecting the Ultima-inspired overland world.
- **[World Mutation & Persistence Blueprint (`WORLD_MUTATION_BLUEPRINT.md`)](file:///c:/git/eatscharted/WORLD_MUTATION_BLUEPRINT.md):** Declarative Eatscript diff persistence, heightfield terraforming, and modular architectural placement.
- **[Encharted Fork Charter (`ENCHARTED_CHARTER.md`)](file:///c:/git/eatscharted/ENCHARTED_CHARTER.md):** Architectural separation between Eatscharted (macro living cartography RPG) and Encharted (seamless 1:1 living world RPG).
- **[Agent & Build Safety Rules (`AGENTS.md`)](file:///c:/git/eatscharted/AGENTS.md):** Strict policies regarding manual testing and Windows GPU process safety.
