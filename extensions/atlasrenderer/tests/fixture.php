<?php
declare(strict_types=1);

/** Stable ABW1 fixture: real 3D voxels, slab, stair, fence, glass and water. */
function atlasFixture(int $bits = 4):string{
    $models = [
        [[0, 0, 0, 0], []],
        [[95, 150, 75, 255], [[0, 0, 0, 1, 1, 1]]],
        [[185, 125, 65, 255], [[0, 0, 0, 1, 1, 1]]],
        [[170, 170, 175, 255], [[0, 0, 0, 1, 0.5, 1]]],
        [[185, 125, 65, 255], [[0, 0, 0, 1, 0.5, 1], [0.5, 0.5, 0, 1, 1, 1]]],
        [[120, 185, 220, 90], [[0, 0, 0, 1, 1, 1]]],
        [[35, 100, 205, 130], [[0, 0, 0, 1, 0.875, 1]]],
        [[130, 90, 50, 255], [[0.375, 0, 0.375, 0.625, 1, 0.625], [0, 0.375, 0.4375, 1, 0.5625, 0.5625], [0, 0.75, 0.4375, 1, 0.9375, 0.5625]]],
    ];
    $blob = "ABW1" . pack("V3", 0, 32, count($models));
    foreach($models as [$rgba, $boxes]){
        $blob .= pack("C6v3", ...array_merge($rgba, [0, count($boxes), 65535, 65535, 65535]));
        foreach($boxes as $box){ $blob .= pack("g6", ...$box); }
    }
    $blob .= pack("vV", 0, 1) . pack("V2Cv", 0, 0, 0, 1) . pack("VC", 0, 1);
    $palette = $bits >= 3 ? range(0, 7) : range(0, (1 << $bits) - 1);
    $words = array_fill(0, (int) ceil(4096 / (intdiv(32, $bits))), 0);
    for($x = 0; $x < 16; ++$x){
        for($z = 0; $z < 16; ++$z){
            for($y = 0; $y < 16; ++$y){
                $id = $y < 3 ? 1 : 0;
                if($x >= 5 && $x <= 10 && $z >= 5 && $z <= 10 && $y >= 3 && $y < 8){ $id = 2; }
                if($x === 5 && $z === 5 && $y === 8){ $id = 3; }
                if($x === 6 && $z === 5 && $y === 8){ $id = 4; }
                if($x === 7 && $z === 5 && $y === 8){ $id = 5; }
                if($x === 8 && $z === 5 && $y === 8){ $id = 6; }
                if($x === 9 && $z === 5 && $y === 8){ $id = 7; }
                $id %= count($palette);
                $index = ($x << 8) | ($z << 4) | $y;
                $per = intdiv(32, $bits);
                $words[intdiv($index, $per)] |= $id << (($index % $per) * $bits);
            }
        }
    }
    $raw = pack("V*", ...$words);
    $blob .= pack("Cv", $bits, count($palette)) . pack("V*", ...$palette) . pack("V", strlen($raw)) . $raw;
    $blob .= pack("CvVV", 0, 1, 0x80bb80, 0);
    return $blob;
}
