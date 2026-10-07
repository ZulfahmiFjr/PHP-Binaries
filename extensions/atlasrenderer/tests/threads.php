<?php

declare(strict_types=1);

require __DIR__ . "/fixture.php";
if(!PHP_ZTS || !extension_loaded("pmmpthread")){ throw new RuntimeException("ZTS pmmpthread is required"); }
final class AtlasThreadProbe extends pmmp\thread\Thread{
    public string $digest = "";
    public string $error = "";
    public function __construct(private string $snapshot, private float $centre){ }
    public function run():void{
        try{
            if(ATLAS_RENDERER_API_VERSION !== 2){ throw new RuntimeException("Worker API missing"); }
            $result = atlas_render($this->snapshot, ["width" => 128, "height" => 128, "x" => $this->centre, "y" => 4, "z" => 8, "scale" => 5]);
            if(strlen($result["rgb"]) !== 49152 || count(atlas_split_tiles($result["rgb"], 128, 128)) !== 1){ throw new RuntimeException("Worker result invalid"); }
            $this->digest = hash("sha256", $result["rgb"]);
        }catch(Throwable $error){ $this->error = $error->getMessage(); }
    }
}
$left = new AtlasThreadProbe(atlasFixture(), 8.0);
$right = new AtlasThreadProbe(atlasFixture(), 12.0);
$left->start(pmmp\thread\Thread::INHERIT_NONE);
$right->start(pmmp\thread\Thread::INHERIT_NONE);
$left->join(); $right->join();
if($left->error !== "" || $right->error !== "" || strlen($left->digest) !== 64 || strlen($right->digest) !== 64 || $left->digest === $right->digest){ throw new RuntimeException("ZTS worker failure: " . $left->error . " " . $right->error); }
echo "Two concurrent managed ZTS threads rendered independent viewports: OK\n";
