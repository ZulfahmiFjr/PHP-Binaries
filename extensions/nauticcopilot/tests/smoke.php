<?php

if (!extension_loaded('nauticcopilot')) {
    echo "Extension nauticcopilot is not loaded\n";
    exit(1);
}

// 1. Distance test
$d = nautic_copilot_distance(0, 0, 0, 10, 0, 10, 20, 0, 20, 25, 10, 25);
if (!is_float($d)) {
    echo "FAIL: nautic_copilot_distance did not return float\n";
    exit(1);
}

// 2. Yaw delta test
$yd = nautic_copilot_yaw_delta(10.0, 350.0);
if (abs($yd - (-20.0)) > 1e-5) {
    echo "FAIL: nautic_copilot_yaw_delta mismatch ($yd != -20.0)\n";
    exit(1);
}

// 3. Guidance test
$g = nautic_copilot_guidance(0, 5, 0, 0, 0, 0, 0, 10, 10, 10, 0.45, 0.20, 0.40);
if (!is_array($g) || !isset($g['forward'], $g['sideways'], $g['vertical'], $g['targetYaw'], $g['throttle'])) {
    echo "FAIL: nautic_copilot_guidance output invalid structure\n";
    exit(1);
}

// 4. Graph nearest test
$nodes = [
    0 => [0.0, 10.0, 0.0],
    1 => [50.0, 10.0, 50.0]
];
$nearest = nautic_copilot_nearest($nodes, 2.0, 2.0);
if ($nearest !== 0) {
    echo "FAIL: nautic_copilot_nearest returned $nearest (expected 0)\n";
    exit(1);
}

// 5. Graph path test
$edges = [
    0 => [1],
    1 => [0]
];
$edgeHeights = [
    '0:1' => 12.0
];
$path = nautic_copilot_path($nodes, $edges, $edgeHeights, 0, 1, 0);
if (count($path) !== 2 || $path[0][0] !== 0.0 || $path[1][0] !== 50.0) {
    echo "FAIL: nautic_copilot_path invalid path\n";
    exit(1);
}

echo "OK: NauticCopilot extension smoke tests passed successfully.\n";
