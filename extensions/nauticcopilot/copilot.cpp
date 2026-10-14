#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "copilot.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <cstring>
#include <cstdio>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace nautic {

constexpr double PI = 3.14159265358979323846;

static inline double rect_distance(double x, double z, double minX, double minZ, double maxX, double maxZ) {
    double dx = 0.0;
    if (x < minX) dx = minX - x;
    else if (x > maxX) dx = x - maxX;

    double dz = 0.0;
    if (z < minZ) dz = minZ - z;
    else if (z > maxZ) dz = z - maxZ;

    return dx * dx + dz * dz;
}

double swept_distance(
    double ax, double ay, double az,
    double bx, double by, double bz,
    double minX, double minY, double minZ,
    double maxX, double maxY, double maxZ
) {
    double dx = bx - ax;
    double dz = bz - az;
    double t0 = 0.0, t1 = 1.0;
    bool cross = true;

    double starts[2] = {ax, az};
    double deltas[2] = {dx, dz};
    double mins[2] = {minX, minZ};
    double maxs[2] = {maxX, maxZ};

    for (int i = 0; i < 2; ++i) {
        if (std::abs(deltas[i]) < 1e-9) {
            if (starts[i] < mins[i] || starts[i] > maxs[i]) {
                cross = false;
                break;
            }
            continue;
        }
        double lo = (mins[i] - starts[i]) / deltas[i];
        double hi = (maxs[i] - starts[i]) / deltas[i];
        if (lo > hi) std::swap(lo, hi);
        if (lo > t0) t0 = lo;
        if (hi < t1) t1 = hi;
        if (t0 > t1) {
            cross = false;
            break;
        }
    }

    if (cross) return 0.0;

    double d1 = rect_distance(ax, az, minX, minZ, maxX, maxZ);
    double d2 = rect_distance(bx, bz, minX, minZ, maxX, maxZ);
    double dist = d1 < d2 ? d1 : d2;

    double length = dx * dx + dz * dz;
    if (length > 1e-9) {
        double xs[2] = {minX, maxX};
        double zs[2] = {minZ, maxZ};
        for (double x : xs) {
            for (double z : zs) {
                double rawT = ((x - ax) * dx + (z - az) * dz) / length;
                double t = rawT < 0.0 ? 0.0 : (rawT > 1.0 ? 1.0 : rawT);
                double px = x - ax - t * dx;
                double pz = z - az - t * dz;
                double d = px * px + pz * pz;
                if (d < dist) dist = d;
            }
        }
    }

    return dist;
}

double yaw_delta(double from, double to) {
    return std::fmod(std::fmod(to - from + 180.0, 360.0) + 360.0, 360.0) - 180.0;
}

GuidanceOutput guidance_toward(
    double feetX, double feetY, double feetZ,
    double frameYaw, double frameX, double frameY, double frameZ,
    double targetX, double targetY, double targetZ,
    double speed, double reverse, double verticalLimit,
    bool precision, double facing
) {
    auto clampVal = [](double val, double low, double high) -> double {
        return val < low ? low : (val > high ? high : val);
    };

    double dx = targetX - feetX;
    double dz = targetZ - feetZ;
    double distance = std::hypot(dx, dz);
    double vertical = clampVal(((targetY - feetY) * 0.22 - frameY * 4.0) / verticalLimit, -1.0, 1.0);

    if (precision) {
        double vx = clampVal(dx * 0.10 - frameX * 0.4, -0.06, 0.06);
        double vz = clampVal(dz * 0.10 - frameZ * 0.4, -0.06, 0.06);
        double rad = frameYaw * (PI / 180.0);
        double f = -std::sin(rad) * vx + std::cos(rad) * vz;
        double side = -std::cos(rad) * vx - std::sin(rad) * vz;
        vertical = clampVal(vertical, -0.40, 0.40);
        return {f / (f >= 0.0 ? speed : reverse), side / reverse, vertical, facing, 0};
    }

    double yaw = distance > 0.3 ? std::fmod(std::atan2(-dx, dz) * (180.0 / PI) + 360.0, 360.0) : frameYaw;
    double error = std::abs(yaw_delta(frameYaw, yaw));
    double errClamped = error > 90.0 ? 90.0 : error;
    double cosVal = std::cos(errClamped * (PI / 180.0));
    double cos3 = cosVal * cosVal * cosVal;
    double cosFactor = cos3 > 0.18 ? cos3 : 0.18;
    double distFactor = (distance * 0.22) < 0.92 ? (distance * 0.22) : 0.92;
    double forward = distFactor * cosFactor;

    if (distance < 0.35) {
        forward = 0.0;
    } else if (error > 75.0) {
        forward = clampVal(forward, 0.18, 0.22);
    }

    return {forward, 0.0, vertical, yaw, 1};
}

static bool extract_point(zval *val, double &x, double &y, double &z) {
    if (!val) return false;
    if (Z_TYPE_P(val) == IS_ARRAY) {
        HashTable *pt = Z_ARRVAL_P(val);
        zval *zx = zend_hash_index_find(pt, 0);
        zval *zy = zend_hash_index_find(pt, 1);
        zval *zz = zend_hash_index_find(pt, 2);
        if (!zx || !zy || !zz) {
            zx = zend_hash_str_find(pt, "x", 1);
            zy = zend_hash_str_find(pt, "y", 1);
            zz = zend_hash_str_find(pt, "z", 1);
        }
        if (zx && zy && zz) {
            x = zval_get_double(zx);
            y = zval_get_double(zy);
            z = zval_get_double(zz);
            return true;
        }
    } else if (Z_TYPE_P(val) == IS_OBJECT) {
        zval rvX, rvY, rvZ;
        zval *zx = zend_read_property(Z_OBJCE_P(val), Z_OBJ_P(val), "x", 1, 1, &rvX);
        zval *zy = zend_read_property(Z_OBJCE_P(val), Z_OBJ_P(val), "y", 1, 1, &rvY);
        zval *zz = zend_read_property(Z_OBJCE_P(val), Z_OBJ_P(val), "z", 1, 1, &rvZ);
        if (zx && zy && zz) {
            x = zval_get_double(zx);
            y = zval_get_double(zy);
            z = zval_get_double(zz);
            return true;
        }
    }
    return false;
}

int graph_nearest(HashTable *nodesHt, double px, double pz) {
    if (!nodesHt) return -1;
    int bestId = -1;
    double bestDist = 1e18;
    zend_ulong num_key;
    zend_string *str_key;
    zval *val;

    ZEND_HASH_FOREACH_KEY_VAL(nodesHt, num_key, str_key, val) {
        double x = 0, y = 0, z = 0;
        if (extract_point(val, x, y, z)) {
            double d = (px - x) * (px - x) + (pz - z) * (pz - z);
            if (d < bestDist) {
                bestDist = d;
                bestId = (int)num_key;
            }
        }
    } ZEND_HASH_FOREACH_END();

    return bestId;
}

std::vector<Point3D> graph_dijkstra(
    HashTable *nodesHt,
    HashTable *edgesHt,
    HashTable *edgeHeightsHt,
    int from,
    int to,
    int tick,
    HashTable *disabledHt,
    double maxHeight
) {
    if (!nodesHt || !edgesHt || !edgeHeightsHt) return {};

    std::unordered_map<int, Point3D> nodeMap;
    zend_ulong num_key;
    zend_string *str_key;
    zval *val;

    ZEND_HASH_FOREACH_KEY_VAL(nodesHt, num_key, str_key, val) {
        double x = 0, y = 0, z = 0;
        if (extract_point(val, x, y, z)) {
            nodeMap[(int)num_key] = {x, y, z};
        }
    } ZEND_HASH_FOREACH_END();

    if (nodeMap.find(from) == nodeMap.end() || nodeMap.find(to) == nodeMap.end()) {
        return {};
    }

    std::unordered_map<int, std::vector<int>> adj;
    ZEND_HASH_FOREACH_KEY_VAL(edgesHt, num_key, str_key, val) {
        int u = (int)num_key;
        if (Z_TYPE_P(val) == IS_ARRAY) {
            zval *nbr;
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(val), nbr) {
                adj[u].push_back((int)zval_get_long(nbr));
            } ZEND_HASH_FOREACH_END();
        }
    } ZEND_HASH_FOREACH_END();

    auto getEdgeHeight = [&](int a, int b) -> double {
        int lo = a < b ? a : b;
        int hi = a > b ? a : b;
        char key[64];
        snprintf(key, sizeof(key), "%d:%d", lo, hi);
        zval *v = zend_hash_str_find(edgeHeightsHt, key, strlen(key));
        if (v) return zval_get_double(v);
        auto itA = nodeMap.find(a);
        auto itB = nodeMap.find(b);
        if (itA != nodeMap.end() && itB != nodeMap.end()) {
            return itA->second.y > itB->second.y ? itA->second.y : itB->second.y;
        }
        return 0.0;
    };

    auto isDisabled = [&](int a, int b) -> bool {
        if (!disabledHt) return false;
        int lo = a < b ? a : b;
        int hi = a > b ? a : b;
        char key[64];
        snprintf(key, sizeof(key), "%d:%d", lo, hi);
        zval *v = zend_hash_str_find(disabledHt, key, strlen(key));
        if (v) {
            return zval_get_long(v) > tick;
        }
        return false;
    };

    using Pair = std::pair<double, int>;
    std::priority_queue<Pair, std::vector<Pair>, std::greater<Pair>> pq;
    std::unordered_map<int, double> cost;
    std::unordered_map<int, int> parents;
    std::unordered_map<int, bool> closed;

    cost[from] = 0.0;
    pq.push({0.0, from});

    while (!pq.empty()) {
        auto [c, u] = pq.top();
        pq.pop();
        if (closed[u]) continue;
        closed[u] = true;

        if (u == to) {
            std::vector<int> ids;
            int curr = to;
            ids.push_back(curr);
            while (parents.find(curr) != parents.end()) {
                curr = parents[curr];
                ids.push_back(curr);
            }
            std::reverse(ids.begin(), ids.end());

            std::vector<Point3D> result;
            for (size_t idx = 0; idx < ids.size(); ++idx) {
                int id = ids[idx];
                double height = nodeMap[id].y;
                if (idx > 0) {
                    double h1 = getEdgeHeight(id, ids[idx - 1]);
                    if (h1 > height) height = h1;
                }
                if (idx + 1 < ids.size()) {
                    double h2 = getEdgeHeight(id, ids[idx + 1]);
                    if (h2 > height) height = h2;
                }
                result.push_back({nodeMap[id].x, height, nodeMap[id].z});
            }
            return result;
        }

        for (int next : adj[u]) {
            if (closed[next] || isDisabled(u, next)) continue;
            double h = getEdgeHeight(u, next);
            if (h > maxHeight) continue;
            const auto& pU = nodeMap[u];
            const auto& pN = nodeMap[next];
            double dist = std::hypot(pU.x - pN.x, pU.y - pN.y, pU.z - pN.z);
            double excess = h > 36.0 ? (h - 36.0) * 3.0 : 0.0;
            double newCost = c + dist + excess;

            auto it = cost.find(next);
            if (it == cost.end() || newCost < it->second) {
                cost[next] = newCost;
                parents[next] = u;
                pq.push({newCost, next});
            }
        }
    }

    return {};
}

}
