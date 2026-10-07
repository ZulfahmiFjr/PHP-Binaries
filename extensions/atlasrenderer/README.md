# AtlasRenderer API 1

Independent PHP extension, no FFI. C++17, no process-wide mutable renderer state.
`atlas_render(string $snapshot, array $camera): array` returns RGB bytes, pixel
hit coordinates (little-endian signed int32 X/Y/Z), preview flags and render timing.
`atlas_split_tiles(string $rgb, int $width, int $height): array` returns row-major
128x128 RGB images. INT32_MIN indicates no selectable hit. Preview flags: 1 for
generator terrain, 2 for missing data or an exhausted traversal budget.

ABW1 is a versioned little-endian format owned by AtlasBoard, not permanent
PocketMine chunk storage. Header: magic, int32 minY/maxY, uint32 model count.
Each model: RGBA (4 bytes), tint byte, box count byte, three uint16 texture IDs
(top/side/bottom; 65535 means flat color), then six float32 values per box.
Texture count uint16 followed by 16x16 RGBA images. Chunk count uint32 followed
by int32 X/Z, preview byte, uint16 section count. Each section contains int32 Y,
layer count byte, block palettes, and a biome-color palette. Palette: bits byte,
uint16 entry count, uint32 entries, uint32 word-byte count, little-endian words.
Block palette entries index the model table. Coordinates are XZY, matching the
verified chunkutils2 layout. Biome palette entries store R | G<<8 | B<<16.

The renderer traces orthographic rays through voxel sections and intersects
model boxes, with side lighting, alpha compositing and texture sampling. Unknown
regions are hatched and cannot be selected as actual world terrain. Model shapes
and asset fidelity depend on the supplied pack. This is not a complete port of
Dynmap lighting or Java custom block renderers.

Build through compile.sh with --enable-atlasrenderer, or phpize/configure for a
matching PHP runtime. Tests/smoke.php validates palette widths, input boundaries
and exact reconstruction of 15 tiles. The Android workflow executes the static
aarch64 binary through QEMU, including an unchanged NauticRenderer regression.
QEMU tests do not establish client visuals, Android thermal behavior or FPS.
