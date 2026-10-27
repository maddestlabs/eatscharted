# Eatscharted (`c:\git\eatscharted`)

**Eatscharted** is a procedural 2.5D "Living Cartography" action-RPG and world-exploration engine built on C++20, WebGPU (`wgpu-native` / Google Dawn / Emscripten), `miniaudio`, and the Eatscript DSL.

It combines:
- **Watabou's *Perilous Shores* Procedural Cartography:** Voronoi/Delaunay dual-mesh world generation, fractal coastlines with concentric ink water-lining rings, topographic slope hachures, clumped ink-wash forests, spline roads/rivers, watercolor biome washes, and curved calligraphic map labels.
- ***Cat Quest*'s 2.5D Tabletop Map Gameplay:** High-angle 2.5D exploration directly across an illustrated parchment world map populated by shadow-casting 2.5D paper-cutout standees, continuous SDF collision, seamless land-to-sea sailing, ground-projected combat AoE telegraphs, zoom-to-atlas camera transitions, and *One-Page Dungeon* interiors.

## Documentation

- **[Architectural Blueprint (`BLUEPRINT.md`)](file:///c:/git/eatscharted/BLUEPRINT.md):** Full technical specification, WGSL shader design, data structures, repository layout, and 5-phase implementation roadmap.
- **[Agent & Build Safety Rules (`AGENTS.md`)](file:///c:/git/eatscharted/AGENTS.md):** Strict policies regarding manual testing and Windows GPU process safety.
