# Eatscharted (`c:\git\eatscharted`)

**Eatscharted** is a native C++20 / WebGPU 2.5D "Living Cartography" action-RPG and world-exploration engine built on the [Eatsbits](file:///c:/git/eatsbits/engine-blueprint.md) and [Eatsrealms](file:///c:/git/eatsrealms/README.md) foundation.

It synthesizes:
- **Watabou's *Perilous Shores* Procedural Cartography:** Voronoi/Delaunay dual-mesh world generation, fractal coastlines with concentric ink water-lining rings, topographic slope hachures aligned with $\nabla h$, clumped ink-wash forests, cased royal highways & dashed wilderness trails, watercolor biome washes with pigment edge-pooling, and 3D curved calligraphic map banners.
- ***Cat Quest*'s 2.5D Tabletop Map Gameplay:** High-angle 2.5D exploration directly across an undulating parchment world map populated by shadow-casting 2.5D paper-cutout standees, continuous SDF collision, seamless land-to-sea caravel sailing, ground-projected combat AoE telegraphs, zoom-to-atlas camera transitions, and Watabou *One-Page Dungeon* cross-hatched interiors.

## Building & Running

```powershell
# Build and launch interactive 2.5D Living Cartography viewer
.\build.ps1

# Compile only without launching
.\build.ps1 -NoRun -NoLaunch

# Render headless PNG preview snapshot
.\build.ps1 -RenderPreview
```

## Controls (*Cat Quest* Tabletop & Cartographic Atlas)

- **`W / A / S / D` or Arrow Keys:** Move Hero across land, or sail the inked Caravel across bays and shoals
- **`Left Shift`:** *Cat Quest* Dodge-Roll / Sprint
- **`Space` / `J` / `Left Click`:** Melee Sword Slash (cleaves nearby enemies with knockback & floating damage numbers)
- **`K` / `Right Click`:** Cast Arcane Nova (projects a glowing runic AoE circle onto the parchment map)
- **`E` / `Enter`:** Enter / Exit nearby **Watabou *One-Page Dungeon*** portal (`SUNKEN SANCTUM`)
- **`M` / `Z`:** Toggle **Zoom-to-Atlas** mode (smoothly transitions between the 2.5D action camera and a high-altitude *Perilous Shores* cartographic map view)
- **`1 / 2 / 3 / 4`:** Switch Chart Recipe:
  - `1`: `scenes/perilous_shores.eats` (*The Perilous Shores* Peninsula, Bay & Barrier Islands)
  - `2`: `scenes/shattered_isles.eats` (*The Shattered Sea* Nautical Archipelago)
  - `3`: `scenes/highland_kingdom.eats` (*Silvervale Realm* Twin-Ridge River Valley)
  - `4`: `scenes/sunken_sanctum_dungeon.eats` (*The Sunken Sanctum* Cross-Hatched One-Page Dungeon)

## Documentation

- **[Architectural Blueprint (`BLUEPRINT.md`)](file:///c:/git/eatscharted/BLUEPRINT.md):** Full technical specification, WGSL shader design, data structures, and module layout.
- **[Agent & Build Safety Rules (`AGENTS.md`)](file:///c:/git/eatscharted/AGENTS.md):** Strict policies regarding manual testing and Windows GPU process safety.
