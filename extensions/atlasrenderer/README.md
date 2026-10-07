# AtlasRenderer API 2

Independent PHP extension, no FFI. C++17, no process-wide mutable renderer state.
`atlas_render(string $snapshot, array $camera, ?array $previous = null): array` returns RGB bytes, pixel
hit coordinates (little-endian signed int32 X/Y/Z), preview flags and render timing.
`atlas_split_tiles(string $rgb, int $width, int $height): array` returns row-major
128x128 RGB images. INT32_MIN indicates no selectable hit. Preview flags: 1 for
generator terrain, 2 for missing data or an exhausted traversal budget.

ABW1 is a versioned little-endian format owned by AtlasBoard, not permanent
PocketMine chunk storage. Header: magic, int32 minY/maxY, uint32 model count.
Each model: RGBA (4 bytes), flags byte, box count byte, three uint16 texture IDs
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

API 2 supports partial renders: camera.region is [left, top, right, bottom]
(exclusive right/bottom); pass previous rgb/hits/preview buffers. Pixels outside
the rectangle remain byte-identical. Model flags use low two bits for tint
(0 none, 1 top grass, 2 foliage, 3 water) and bit 2 for connected nine-box fences.
Textured RGB is multiplied by the model colour, supporting dyed blocks.

Projection version 2 corrects the horizontal signs of the downward ray direction.
The image right/up vectors and ray direction now form an orthogonal basis, so
higher geometry rises on screen and the 45-degree view no longer collapses.
ABW1 and API version 2 remain compatible. Detect the correction using
ATLAS_RENDERER_PROJECTION_VERSION === 2. AtlasBoard 1.1.1 compensates for the
older binary when this constant is absent; the corrected binary uses the normal
single-render path and is preferred for performance. Roof projection tests cover
20/45/60/80-degree views and opposite azimuths. NauticRenderer is unchanged.

