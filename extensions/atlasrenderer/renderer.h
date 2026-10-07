#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace atlas {
constexpr int API_VERSION = 2;
struct Camera {
    int width = 640, height = 384;
    double x = 0, y = 64, z = 0, scale = 4, azimuth = 135, inclination = 60;
    int max_steps = 2048;
};
struct Result {
    std::string rgb, hits, preview;
    uint64_t traced_voxels = 0;
};
Result render(const std::string &snapshot, const Camera &camera, const Result *previous = nullptr, const std::array<int,4> *region = nullptr);
std::vector<std::string> split_tiles(const std::string &rgb, int width, int height);
std::vector<double> project(const Camera &camera, double x, double y, double z);
}
