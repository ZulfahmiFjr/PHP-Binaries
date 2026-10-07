<?php
declare(strict_types=1);
foreach([0, -1, 123456789, PHP_INT_MAX, PHP_INT_MIN] as $seed){
    $args = [0.0, 0.0, 135.0, $seed, 110, 70, 62, 45, 8, 10, 3, 18, 28, [], []];
    $first = nautic_render_map(...$args);
    if(strlen($first) !== 49152 || $first !== nautic_render_map(...$args)){ throw new RuntimeException("Nautic renderer regression"); }
}
echo "NauticRenderer buffer and deterministic seed checks passed\n";
