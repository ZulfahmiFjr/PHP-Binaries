#ifndef NAUTIC_COPILOT_H
#define NAUTIC_COPILOT_H

#include <vector>
#include <string>
#include <unordered_map>
#include "php.h"

namespace nautic {

struct Point3D {
    double x, y, z;
};

struct GuidanceOutput {
    double forward;
    double sideways;
    double vertical;
    double targetYaw;
    int throttle;
};

double swept_distance(
    double ax, double ay, double az,
    double bx, double by, double bz,
    double minX, double minY, double minZ,
    double maxX, double maxY, double maxZ
);

double yaw_delta(double from, double to);

GuidanceOutput guidance_toward(
    double feetX, double feetY, double feetZ,
    double frameYaw, double frameX, double frameY, double frameZ,
    double targetX, double targetY, double targetZ,
    double speed, double reverse, double verticalLimit,
    bool precision, double facing
);

int graph_nearest(HashTable *nodesHt, double px, double pz);

std::vector<Point3D> graph_dijkstra(
    HashTable *nodesHt,
    HashTable *edgesHt,
    HashTable *edgeHeightsHt,
    int from,
    int to,
    int tick,
    HashTable *disabledHt,
    double maxHeight
);

}

#endif
