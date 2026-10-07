<?php
declare(strict_types=1);
require __DIR__ . "/fixture.php";
if(!extension_loaded("atlasrenderer") || ATLAS_RENDERER_API_VERSION !== 2){ throw new RuntimeException("AtlasRenderer API 2 missing"); }
$camera = ["width" => 128, "height" => 128, "x" => 8, "y" => 4, "z" => 8, "scale" => 5];
foreach([1, 2, 3, 4, 5, 6, 8, 16] as $bits){
    $result = atlas_render(atlasFixture($bits), $camera);
    if(strlen($result["rgb"]) !== 49152 || strlen($result["hits"]) !== 196608 || strlen($result["preview"]) !== 16384){ throw new RuntimeException("Invalid buffers"); }
    if(atlas_split_tiles($result["rgb"], 128, 128) !== [$result["rgb"]]){ throw new RuntimeException("Tile mismatch"); }
}
$large = atlas_render(atlasFixture(), array_merge($camera, ["width" => 640, "height" => 384]));
$tiles = atlas_split_tiles($large["rgb"], 640, 384);
if(count($tiles) !== 15){ throw new RuntimeException("Expected 15 tiles"); }
$reconstructed = "";
for($y = 0; $y < 384; ++$y){
    for($x = 0; $x < 5; ++$x){ $reconstructed .= substr($tiles[intdiv($y, 128) * 5 + $x], ($y % 128) * 384, 384); }
}
if($reconstructed !== $large["rgb"]){ throw new RuntimeException("Tile seam mismatch"); }
foreach(["", "ABW1", substr(atlasFixture(), 0, -1), atlasFixture() . "x"] as $invalid){
    try{ atlas_render($invalid, $camera); throw new RuntimeException("Invalid input accepted"); }catch(ValueError $e){}
}
foreach([NAN, INF, -1, 1e30, 128.5] as $invalid){
    try{ atlas_render(atlasFixture(), array_merge($camera, ["width" => $invalid])); throw new RuntimeException("Invalid camera accepted"); }catch(ValueError $e){}
}
$regionCamera = array_merge($camera, ["width" => 640, "height" => 384, "region" => [256, 128, 384, 256]]);
$partial = atlas_render(atlasFixture(), $regionCamera, $large);
if($partial["rgb"] !== $large["rgb"] || $partial["hits"] !== $large["hits"] || $partial["preview"] !== $large["preview"] || $partial["traced_voxels"] >= $large["traced_voxels"]){ throw new RuntimeException("Partial render mismatch"); }
try{ atlas_render(atlasFixture(), $regionCamera, ["rgb" => "", "hits" => "", "preview" => ""]); throw new RuntimeException("Invalid base accepted"); }catch(ValueError $e){}
try{ atlas_render(atlasFixture(), $regionCamera); throw new RuntimeException("Region without base accepted"); }catch(ValueError $e){}
$centreHit = array_values(unpack("V3", $large["hits"], (192 * 640 + 320) * 12));
if($centreHit === [2147483648,2147483648,2147483648]){ throw new RuntimeException("Visible geometry has no hit"); }
$out = getenv("ATLAS_TEST_OUTPUT");
if(is_string($out) && $out !== ""){
    file_put_contents($out, "P6\n640 384\n255\n" . $large["rgb"]);
}
echo json_encode(["api" => ATLAS_RENDERER_API_VERSION, "tiles" => count($tiles), "render_ms" => $large["render_ms"], "rgb_bytes" => strlen($large["rgb"]), "hit_bytes" => strlen($large["hits"]), "peak_php_bytes" => memory_get_peak_usage(true)], JSON_PRETTY_PRINT) . "\n";
