#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// ==========================================
// BAGIAN 1: ENGINE RANDOM XORSHIFT128 (TRUE 64-BIT PHP CLONE)
// ==========================================
typedef struct {
    int64_t x;
    int64_t y;
    int64_t z;
    int64_t w;
    int64_t seed;
} PMMP_Random;

static void random_set_seed(PMMP_Random* r, int64_t seed) {
    r->seed = seed;
    r->x = 123456789LL ^ seed;
    r->y = 362436069LL ^ (seed << 17) | ((seed >> 15) & 0x7fffffffLL) & 0xffffffffLL;
    r->z = 521288629LL ^ (seed << 31) | ((seed >> 1) & 0x7fffffffLL) & 0xffffffffLL;
    r->w = 88675123LL  ^ (seed << 18) | ((seed >> 14) & 0x7fffffffLL) & 0xffffffffLL;
}

static int32_t random_next_signed_int(PMMP_Random* r) {
    int64_t t = (r->x ^ (r->x << 11)) & 0xffffffffLL;
    r->x = r->y;
    r->y = r->z;
    r->z = r->w;

    int64_t w_shift = (r->w >> 19) & 0x7fffffffLL;
    int64_t t_shift = (t >> 8) & 0x7fffffffLL;

    r->w = (r->w ^ w_shift ^ (t ^ t_shift)) & 0xffffffffLL;
    return (int32_t)r->w;
}

static int32_t random_next_int(PMMP_Random* r) {
    return random_next_signed_int(r) & 0x7fffffff;
}

static double random_next_float(PMMP_Random* r) {
    return (double)random_next_int(r) / 2147483647.0;
}

static int32_t random_next_bounded_int(PMMP_Random* r, int32_t bound) {
    return random_next_int(r) % bound;
}

static int32_t random_next_range(PMMP_Random* r, int32_t start, int32_t end) {
    return start + (random_next_int(r) % (end + 1 - start));
}

// ==========================================
// BAGIAN 2: SIMPLEX NOISE (ANTI-TUMPAH 64-BIT)
// ==========================================
typedef struct {
    int32_t octaves;
    double persistence;
    double expansion;
    double offsetX;
    double offsetY;
    double offsetZ;
    int32_t perm[512];
} PMMP_Simplex;

static const int grad3[12][3] = {
    {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
    {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
    {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}
};

static const double F2 = 0.5 * (1.73205080756887729352 - 1.0);
static const double G2 = (3.0 - 1.73205080756887729352) / 6.0;
static const double G22 = ((3.0 - 1.73205080756887729352) / 6.0) * 2.0 - 1.0;

static void simplex_init(PMMP_Simplex* s, PMMP_Random* r, int32_t octaves, double persistence, double expansion) {
    s->octaves = octaves;
    s->persistence = persistence;
    s->expansion = expansion;

    s->offsetX = random_next_float(r) * 256.0;
    s->offsetY = random_next_float(r) * 256.0;
    s->offsetZ = random_next_float(r) * 256.0;

    for (int i = 0; i < 512; ++i) s->perm[i] = 0;
    for (int i = 0; i < 256; ++i) s->perm[i] = random_next_bounded_int(r, 256);

    for (int i = 0; i < 256; ++i) {
        int pos = random_next_bounded_int(r, 256 - i) + i;
        int old = s->perm[i];
        s->perm[i] = s->perm[pos];
        s->perm[pos] = old;
        s->perm[i + 256] = s->perm[i];
    }
    random_next_signed_int(r);
}

static double simplex_get_noise_2d(PMMP_Simplex* s, double x, double y) {
    x += s->offsetX;
    y += s->offsetY;

    double skew = (x + y) * F2;

    int64_t i = (int64_t)(x + skew);
    int64_t j = (int64_t)(y + skew);
    double t = (double)(i + j) * G2;

    double x0 = x - ((double)i - t);
    double y0 = y - ((double)j - t);

    int64_t i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else { i1 = 0; j1 = 1; }

    double x1 = x0 - (double)i1 + G2;
    double y1 = y0 - (double)j1 + G2;
    double x2 = x0 + G22;
    double y2 = y0 + G22;

    int ii = i & 255;
    int jj = j & 255;

    double n = 0.0;
    double t0 = 0.5 - x0 * x0 - y0 * y0;
    if (t0 > 0.0) {
        int gi0 = s->perm[ii + s->perm[jj]] % 12;
        n += t0 * t0 * t0 * t0 * ((double)grad3[gi0][0] * x0 + (double)grad3[gi0][1] * y0);
    }

    double t1 = 0.5 - x1 * x1 - y1 * y1;
    if (t1 > 0.0) {
        int gi1 = s->perm[ii + i1 + s->perm[jj + j1]] % 12;
        n += t1 * t1 * t1 * t1 * ((double)grad3[gi1][0] * x1 + (double)grad3[gi1][1] * y1);
    }

    double t2 = 0.5 - x2 * x2 - y2 * y2;
    if (t2 > 0.0) {
        int gi2 = s->perm[ii + 1 + s->perm[jj + 1]] % 12;
        n += t2 * t2 * t2 * t2 * ((double)grad3[gi2][0] * x2 + (double)grad3[gi2][1] * y2);
    }
    return 70.0 * n;
}

// ==========================================
// BAGIAN 3: KALKULASI MAP & TERRAIN
// ==========================================
static double clamp_val(double v, double min_val, double max_val) {
    if (v < min_val) return min_val;
    if (v > max_val) return max_val;
    return v;
}

static double smooth_step(double edge0, double edge1, double x) {
    double t = clamp_val((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

static double lerp_val(double a, double b, double t) {
    return a + (b - a) * t;
}

static int64_t mix_seed(int64_t seed, int64_t a, int64_t b) {
    int64_t h = seed ^ (a * 73428767LL) ^ (b * 912367LL);
    h ^= (h << 13);
    h ^= (h >> 17);
    h ^= (h << 5);
    return h;
}

void nautic_render_map_core(
    double centerX, double centerZ, double yaw, int64_t worldSeed,
    int plotSize, int gap, int seaLevel, int oceanBase,
    int oceanVar, int islandHeightPeak, int beachWidth,
    int radMin, int radMax,
    int otherCount, double* otherPlayers,
    int changeCount, int* changedX, int* changedZ, int* changedType,
    unsigned char* buffer
) {
    PMMP_Random mainRandom;
    random_set_seed(&mainRandom, worldSeed);

    PMMP_Simplex islandNoise;
    simplex_init(&islandNoise, &mainRandom, 4, 1.0 / 64.0, 1.0 / 64.0);

    PMMP_Simplex oceanNoise;
    simplex_init(&oceanNoise, &mainRandom, 3, 1.0 / 96.0, 1.0 / 96.0);

    double rad = yaw * (3.14159265358979323846 / 180.0);
    double cosYaw = cos(rad);
    double sinYaw = sin(rad);

    int cellSize = plotSize + gap;
    int offset = 0;

    for (int y = -64; y < 64; y++) {
        for (int x = -64; x < 64; x++) {
            double rotX = -x * cosYaw + y * sinYaw;
            double rotZ = -x * sinYaw - y * cosYaw;

            int64_t blockX = (int64_t)floor(centerX + rotX);
            int64_t blockZ = (int64_t)floor(centerZ + rotZ);

            double worldX = (double)blockX;
            double worldZ = (double)blockZ;

            int cellX = (int)floor(worldX / cellSize);
            int cellZ = (int)floor(worldZ / cellSize);
            double localX = worldX - (cellX * cellSize);
            double localZ = worldZ - (cellZ * cellSize);

            double oNoise = simplex_get_noise_2d(&oceanNoise, worldX * 0.02, worldZ * 0.02);
            double globalFloorY = oceanBase + (oNoise * oceanVar);
            double finalGroundY = globalFloorY;

            int64_t plotSeed = mix_seed(worldSeed, cellX, cellZ);
            PMMP_Random plotRandom;
            random_set_seed(&plotRandom, plotSeed);

            int baseRadius = random_next_range(&plotRandom, radMin, radMax);
            int cX = random_next_range(&plotRandom, (int)(plotSize * 0.35), (int)(plotSize * 0.65));
            int cZ = random_next_range(&plotRandom, (int)(plotSize * 0.35), (int)(plotSize * 0.65));

            double dx = localX - cX;
            double dz = localZ - cZ;
            double dist = sqrt((dx * dx) + (dz * dz));

            double coastFreq = 0.05;
            double coastNoise = simplex_get_noise_2d(&islandNoise, (worldX + plotSeed) * coastFreq, (worldZ - plotSeed) * coastFreq);
            double irregularRadius = baseRadius + (coastNoise * 5.0);

            double shelfWidth = 35.0;
            double totalRadius = irregularRadius + shelfWidth;

            if (dist < totalRadius) {
                double factor = 1.0 - (dist / totalRadius);
                factor = smooth_step(0.0, 1.0, factor);
                factor = pow(factor, 2.2);

                double targetPeakY = seaLevel + islandHeightPeak;
                double islandShapeY = lerp_val(globalFloorY, targetPeakY, factor);

                double noiseMask = smooth_step(0.0, 0.4, factor);
                double surfaceNoise = simplex_get_noise_2d(&islandNoise, worldX * 0.04, worldZ * 0.04) * 3.5;
                islandShapeY += (surfaceNoise * noiseMask);

                if (islandShapeY > finalGroundY) {
                    finalGroundY = islandShapeY;
                }
            }

            int groundY = (int)finalGroundY;

            int isCustom = 0;
            for(int c = 0; c < changeCount; c++){
                if(changedX[c] == blockX && changedZ[c] == blockZ){
                    isCustom = 1;
                    break;
                }
            }

            if (isCustom == 1) {
                buffer[offset++] = 140;
                buffer[offset++] = 135;
                buffer[offset++] = 130;
            } else if (groundY > seaLevel + beachWidth) {
                buffer[offset++] = 40;
                buffer[offset++] = 170;
                buffer[offset++] = 60;
            } else if (groundY >= seaLevel) {
                buffer[offset++] = 230;
                buffer[offset++] = 220;
                buffer[offset++] = 140;
            } else {
                double waterDepth = (double)(seaLevel - groundY);
                double depthFactor = 1.0 - (waterDepth / 15.0);
                if (depthFactor > 1.0) depthFactor = 1.0;
                if (depthFactor < 0.0) depthFactor = 0.0;

                buffer[offset++] = (unsigned char)(0 * depthFactor);
                buffer[offset++] = (unsigned char)(90 * depthFactor + 45 * (1.0 - depthFactor));
                buffer[offset++] = (unsigned char)(180 * depthFactor + 90 * (1.0 - depthFactor));
            }
        }
    }

    int marker[7][9] = {
        {0,0,0,0,2,0,0,0,0},
        {0,0,0,2,1,2,0,0,0},
        {0,0,2,1,1,1,2,0,0},
        {0,0,2,1,1,1,2,0,0},
        {0,0,2,1,1,1,2,0,0},
        {0,0,2,1,1,1,2,0,0},
        {0,0,0,2,2,2,0,0,0}
    };

    for (int i = 0; i < otherCount; i++) {
        double ox = otherPlayers[i*3];
        double oz = otherPlayers[i*3 + 1];
        double oyaw = otherPlayers[i*3 + 2];

        double dx = ox - centerX;
        double dz = oz - centerZ;

        double screenX = -dx * cosYaw - dz * sinYaw;
        double screenY =  dx * sinYaw - dz * cosYaw;

        if (screenX >= -64 && screenX < 64 && screenY >= -64 && screenY < 64) {
            int cx = (int)round(screenX) + 64;
            int cy = (int)round(screenY) + 64;

            double arrowRad = (oyaw - yaw) * (3.14159265358979323846 / 180.0);
            double aCos = cos(arrowRad);
            double aSin = sin(arrowRad);

            for (int my = 0; my < 7; my++) {
                for (int mx = 0; mx < 9; mx++) {
                    int pixel = marker[my][mx];
                    if (pixel != 0) {
                        double px = mx - 4.0;
                        double py = my - 3.0;

                        int rx = (int)round(px * aCos - py * aSin);
                        int ry = (int)round(px * aSin + py * aCos);

                        int mapX = cx + rx;
                        int mapY = cy + ry;

                        if (mapX >= 0 && mapX < 128 && mapY >= 0 && mapY < 128) {
                            int idx = (mapY * 128 + mapX) * 3;
                            if (pixel == 1) {
                                buffer[idx] = 255; buffer[idx+1] = 255; buffer[idx+2] = 255;
                            } else if (pixel == 2) {
                                buffer[idx] = 255; buffer[idx+1] = 50; buffer[idx+2] = 50;
                            }
                        }
                    }
                }
            }
        }
    }

    for (int my = 0; my < 7; my++) {
        for (int mx = 0; mx < 9; mx++) {
            int pixel = marker[my][mx];
            if (pixel != 0) {
                int cy = my - 3;
                int cx = mx - 4;

                int idx = ((cy + 64) * 128 + (cx + 64)) * 3;

                if (pixel == 1) {
                    buffer[idx] = 255;
                    buffer[idx+1] = 255;
                    buffer[idx+2] = 255;
                } else if (pixel == 2) {
                    buffer[idx] = 0;
                    buffer[idx+1] = 0;
                    buffer[idx+2] = 0;
                }
            }
        }
    }
}
