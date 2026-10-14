#include "copilot.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <cstring>
#include <cstdio>

namespace nautic {

constexpr double PI = 3.14159265358979323846;

static inline double rect_distance(double x, double z, double minX, double minZ, double maxX, double maxZ) {
    double dx = std::max({minX - x, 0.0, x - maxX});
    double dz = std::max({minZ - z, 0.0, z - maxZ});
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
        t0 = std::max(t0, lo);
        t1 = std::min(t1, hi);
        if (t0 > t1) {
            cross = false;
            break;
        }
    }

    if (cross) return 0.0;

    double dist = std::min(
        rect_distance(ax, az, minX, minZ, maxX, maxZ),
        rect_distance(bx, bz, minX, minZ, maxX, maxZ)
    );

    double length = dx * dx + dz * dz;
    if (length > 1e-9) {
        double xs[2] = {minX, maxX};
        double zs[2] = {minZ, maxZ};
        for (double x : xs) {
            for (double z : zs) {
                double t = std::max(0.0, std::min(1.0, ((x - ax) * dx + (z - az) * dz) / length));
                double px = x - ax - t * dx;
                double pz = z - az - t * dz;
                dist = std::min(dist, px * px + pz * pz);
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
    double dx = targetX - feetX;
    double dz = targetZ - feetZ;
    double distance = std::hypot(dx, dz);
    double vertical = std::max(-1.0, std::min(1.0, ((targetY - feetY) * 0.22 - frameY * 4.0) / verticalLimit));

    if (precision) {
        double vx = std::max(-0.06, std::min(0.06, dx * 0.10 - frameX * 0.4));
        double vz = std::max(-0.06, std::min(0.06, dz * 0.10 - frameZ * 0.4));
        double rad = frameYaw * (PI / 180.0);
        double f = -std::sin(rad) * vx + std::cos(rad) * vz;
        double side = -std::cos(rad) * vx - std::sin(rad) * vz;
        vertical = std::max(-0.40, std::min(0.40, vertical));
        return {f / (f >= 0 ? speed : reverse), side / reverse, vertical, facing, 0};
    }

    double yaw = distance > 0.3 ? std::fmod(std::atan2(-dx, dz) * (180.0 / PI) + 360.0, 360.0) : frameYaw;
    double error = std::abs(yaw_delta(frameYaw, yaw));
    double cosVal = std::cos(std::min(90.0, error) * (PI / 180.0));
    double forward = std::min(0.92, distance * 0.22) * std::max(0.18, cosVal * cosVal * cosVal);

    if (distance < 0.35) {
        forward = 0.0;
    } else if (error > 75.0) {
        forward = std::max(0.18, std::min(0.22, forward));
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
        int lo = std::min(a, b);
        int hi = std::max(a, b);
        char key[64];
        snprintf(key, sizeof(key), "%d:%d", lo, hi);
        zval *v = zend_hash_str_find(edgeHeightsHt, key, strlen(key));
        if (v) return zval_get_double(v);
        auto itA = nodeMap.find(a);
        auto itB = nodeMap.find(b);
        if (itA != nodeMap.end() && itB != nodeMap.end()) {
            return std::max(itA->second.y, itB->second.y);
        }
        return 0.0;
    };

    auto isDisabled = [&](int a, int b) -> bool {
        if (!disabledHt) return false;
        int lo = std::min(a, b);
        int hi = std::max(a, b);
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
                if (idx > 0) height = std::max(height, getEdgeHeight(id, ids[idx - 1]));
                if (idx + 1 < ids.size()) height = std::max(height, getEdgeHeight(id, ids[idx + 1]));
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
            double newCost = c + dist + std::max(0.0, h - 36.0) * 3.0;

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
