#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include "php_nauticcopilot.h"
#include "ext/standard/info.h"
}
#include "copilot.h"
#include <cmath>

PHP_FUNCTION(nautic_copilot_distance) {
    double ax, ay, az, bx, by, bz, minX, minY, minZ, maxX, maxY, maxZ;
    ZEND_PARSE_PARAMETERS_START(12, 12)
        Z_PARAM_DOUBLE(ax)
        Z_PARAM_DOUBLE(ay)
        Z_PARAM_DOUBLE(az)
        Z_PARAM_DOUBLE(bx)
        Z_PARAM_DOUBLE(by)
        Z_PARAM_DOUBLE(bz)
        Z_PARAM_DOUBLE(minX)
        Z_PARAM_DOUBLE(minY)
        Z_PARAM_DOUBLE(minZ)
        Z_PARAM_DOUBLE(maxX)
        Z_PARAM_DOUBLE(maxY)
        Z_PARAM_DOUBLE(maxZ)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_DOUBLE(nautic::swept_distance(ax, ay, az, bx, by, bz, minX, minY, minZ, maxX, maxY, maxZ));
}

PHP_FUNCTION(nautic_copilot_yaw_delta) {
    double from, to;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_DOUBLE(from)
        Z_PARAM_DOUBLE(to)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_DOUBLE(nautic::yaw_delta(from, to));
}

PHP_FUNCTION(nautic_copilot_guidance) {
    double feetX, feetY, feetZ, frameYaw, frameX, frameY, frameZ;
    double targetX, targetY, targetZ, speed, reverse, verticalLimit;
    bool precision = false;
    double facing = 0.0;
    bool facing_is_null = true;

    ZEND_PARSE_PARAMETERS_START(13, 15)
        Z_PARAM_DOUBLE(feetX)
        Z_PARAM_DOUBLE(feetY)
        Z_PARAM_DOUBLE(feetZ)
        Z_PARAM_DOUBLE(frameYaw)
        Z_PARAM_DOUBLE(frameX)
        Z_PARAM_DOUBLE(frameY)
        Z_PARAM_DOUBLE(frameZ)
        Z_PARAM_DOUBLE(targetX)
        Z_PARAM_DOUBLE(targetY)
        Z_PARAM_DOUBLE(targetZ)
        Z_PARAM_DOUBLE(speed)
        Z_PARAM_DOUBLE(reverse)
        Z_PARAM_DOUBLE(verticalLimit)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(precision)
        Z_PARAM_DOUBLE_OR_NULL(facing, facing_is_null)
    ZEND_PARSE_PARAMETERS_END();

    double actualFacing = facing_is_null ? frameYaw : facing;
    nautic::GuidanceOutput out = nautic::guidance_toward(
        feetX, feetY, feetZ,
        frameYaw, frameX, frameY, frameZ,
        targetX, targetY, targetZ,
        speed, reverse, verticalLimit,
        precision, actualFacing
    );

    array_init(return_value);
    add_assoc_double(return_value, "forward", out.forward);
    add_assoc_double(return_value, "sideways", out.sideways);
    add_assoc_double(return_value, "vertical", out.vertical);
    add_assoc_double(return_value, "targetYaw", out.targetYaw);
    add_assoc_long(return_value, "throttle", out.throttle);
}

PHP_FUNCTION(nautic_copilot_nearest) {
    HashTable *nodes = nullptr;
    double px, pz;

    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_ARRAY_HT(nodes)
        Z_PARAM_DOUBLE(px)
        Z_PARAM_DOUBLE(pz)
    ZEND_PARSE_PARAMETERS_END();

    int id = nautic::graph_nearest(nodes, px, pz);
    if (id < 0) {
        RETURN_NULL();
    }
    RETURN_LONG(id);
}

PHP_FUNCTION(nautic_copilot_path) {
    HashTable *nodes = nullptr;
    HashTable *edges = nullptr;
    HashTable *edgeHeights = nullptr;
    zend_long from, to, tick;
    HashTable *disabled = nullptr;
    double maxHeight = 1e9;

    ZEND_PARSE_PARAMETERS_START(6, 8)
        Z_PARAM_ARRAY_HT(nodes)
        Z_PARAM_ARRAY_HT(edges)
        Z_PARAM_ARRAY_HT(edgeHeights)
        Z_PARAM_LONG(from)
        Z_PARAM_LONG(to)
        Z_PARAM_LONG(tick)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT_OR_NULL(disabled)
        Z_PARAM_DOUBLE(maxHeight)
    ZEND_PARSE_PARAMETERS_END();

    std::vector<nautic::Point3D> pts = nautic::graph_dijkstra(
        nodes, edges, edgeHeights,
        (int)from, (int)to, (int)tick,
        disabled, maxHeight
    );

    array_init(return_value);
    for (const auto &p : pts) {
        zval ptArr;
        array_init(&ptArr);
        add_next_index_double(&ptArr, p.x);
        add_next_index_double(&ptArr, p.y);
        add_next_index_double(&ptArr, p.z);
        add_next_index_zval(return_value, &ptArr);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_nautic_copilot_distance, 0, 12, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, ax, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, ay, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, az, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, bx, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, by, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, bz, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, minX, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, minY, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, minZ, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, maxX, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, maxY, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, maxZ, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_nautic_copilot_yaw_delta, 0, 2, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, from, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, to, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_nautic_copilot_guidance, 0, 13, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, feetX, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, feetY, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, feetZ, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, frameYaw, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, frameX, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, frameY, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, frameZ, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, targetX, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, targetY, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, targetZ, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, speed, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, reverse, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, verticalLimit, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, precision, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, facing, IS_DOUBLE, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_nautic_copilot_nearest, 0, 3, IS_LONG, 1)
    ZEND_ARG_TYPE_INFO(0, nodes, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, px, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, pz, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_nautic_copilot_path, 0, 6, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, nodes, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, edges, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, edgeHeights, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, from, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, to, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, tick, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, disabled, IS_ARRAY, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxHeight, IS_DOUBLE, 0, "1000000000.0")
ZEND_END_ARG_INFO()

static const zend_function_entry functions[] = {
    PHP_FE(nautic_copilot_distance, arginfo_nautic_copilot_distance)
    PHP_FE(nautic_copilot_yaw_delta, arginfo_nautic_copilot_yaw_delta)
    PHP_FE(nautic_copilot_guidance, arginfo_nautic_copilot_guidance)
    PHP_FE(nautic_copilot_nearest, arginfo_nautic_copilot_nearest)
    PHP_FE(nautic_copilot_path, arginfo_nautic_copilot_path)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(nauticcopilot) {
    REGISTER_LONG_CONSTANT("NAUTIC_COPILOT_API_VERSION", NAUTIC_COPILOT_API_VERSION, CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}

PHP_MINFO_FUNCTION(nauticcopilot) {
    php_info_print_table_start();
    php_info_print_table_row(2, "NauticCopilot support", "enabled");
    php_info_print_table_row(2, "Version", PHP_NAUTICCOPILOT_VERSION);
    php_info_print_table_row(2, "API Version", "1");
    php_info_print_table_end();
}

extern "C" {
zend_module_entry nauticcopilot_module_entry = {
    STANDARD_MODULE_HEADER,
    "nauticcopilot",
    functions,
    PHP_MINIT(nauticcopilot),
    nullptr,
    nullptr,
    nullptr,
    PHP_MINFO(nauticcopilot),
    PHP_NAUTICCOPILOT_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_NAUTICCOPILOT
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(nauticcopilot)
#endif
}
