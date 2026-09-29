#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "php.h"
#include "ext/standard/info.h"
#include "php_nauticrenderer.h"

#define NAUTIC_MAP_WIDTH 128
#define NAUTIC_MAP_HEIGHT 128
#define NAUTIC_MAP_CHANNELS 3
#define NAUTIC_MAP_BUFFER_SIZE (NAUTIC_MAP_WIDTH * NAUTIC_MAP_HEIGHT * NAUTIC_MAP_CHANNELS)

void nautic_render_map_core(
    double centerX, double centerZ, double yaw, int64_t worldSeed,
    int plotSize, int gap, int seaLevel, int oceanBase,
    int oceanVar, int islandHeightPeak, int beachWidth,
    int radMin, int radMax,
    int otherCount, double* otherPlayers,
    int changeCount, int* changedX, int* changedZ, int* changedType,
    unsigned char* buffer
);

PHP_FUNCTION(nautic_add)
{
    zend_long a;
    zend_long b;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(a)
        Z_PARAM_LONG(b)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(a + b);
}

PHP_FUNCTION(nautic_render_map)
{
    double centerX;
    double centerZ;
    double yaw;
    zend_long worldSeed;
    zend_long plotSize;
    zend_long gap;
    zend_long seaLevel;
    zend_long oceanBase;
    zend_long oceanVar;
    zend_long islandHeightPeak;
    zend_long beachWidth;
    zend_long radMin;
    zend_long radMax;
    zval *otherPlayersArray;
    zval *changesArray;

    ZEND_PARSE_PARAMETERS_START(15, 15)
        Z_PARAM_DOUBLE(centerX)
        Z_PARAM_DOUBLE(centerZ)
        Z_PARAM_DOUBLE(yaw)
        Z_PARAM_LONG(worldSeed)
        Z_PARAM_LONG(plotSize)
        Z_PARAM_LONG(gap)
        Z_PARAM_LONG(seaLevel)
        Z_PARAM_LONG(oceanBase)
        Z_PARAM_LONG(oceanVar)
        Z_PARAM_LONG(islandHeightPeak)
        Z_PARAM_LONG(beachWidth)
        Z_PARAM_LONG(radMin)
        Z_PARAM_LONG(radMax)
        Z_PARAM_ARRAY(otherPlayersArray)
        Z_PARAM_ARRAY(changesArray)
    ZEND_PARSE_PARAMETERS_END();

    HashTable *otherHt = Z_ARRVAL_P(otherPlayersArray);
    uint32_t otherCapacity = zend_hash_num_elements(otherHt);
    double *otherPlayers = NULL;
    int otherCount = 0;

    if (otherCapacity > 0) {
        otherPlayers = safe_emalloc((size_t) otherCapacity * 3, sizeof(double), 0);

        zval *playerEntry;
        ZEND_HASH_FOREACH_VAL(otherHt, playerEntry) {
            if (Z_TYPE_P(playerEntry) != IS_ARRAY) {
                continue;
            }

            HashTable *playerHt = Z_ARRVAL_P(playerEntry);
            zval *xValue = zend_hash_str_find(playerHt, "x", sizeof("x") - 1);
            zval *zValue = zend_hash_str_find(playerHt, "z", sizeof("z") - 1);
            zval *yawValue = zend_hash_str_find(playerHt, "yaw", sizeof("yaw") - 1);

            if (xValue == NULL || zValue == NULL || yawValue == NULL) {
                continue;
            }

            otherPlayers[otherCount * 3] = zval_get_double(xValue);
            otherPlayers[otherCount * 3 + 1] = zval_get_double(zValue);
            otherPlayers[otherCount * 3 + 2] = zval_get_double(yawValue);
            otherCount++;
        } ZEND_HASH_FOREACH_END();
    }

    HashTable *changesHt = Z_ARRVAL_P(changesArray);
    uint32_t changeCapacity = zend_hash_num_elements(changesHt);
    int *changedX = NULL;
    int *changedZ = NULL;
    int *changedType = NULL;
    int changeCount = 0;

    if (changeCapacity > 0) {
        changedX = safe_emalloc(changeCapacity, sizeof(int), 0);
        changedZ = safe_emalloc(changeCapacity, sizeof(int), 0);
        changedType = safe_emalloc(changeCapacity, sizeof(int), 0);

        zend_ulong numericKey;
        zend_string *stringKey;
        zval *changeValue;

        ZEND_HASH_FOREACH_KEY_VAL(changesHt, numericKey, stringKey, changeValue) {
            if (stringKey == NULL) {
                continue;
            }

            const char *key = ZSTR_VAL(stringKey);
            char *separator = NULL;
            long bx = strtol(key, &separator, 10);

            if (separator == key || separator == NULL || *separator != ':') {
                continue;
            }

            char *end = NULL;
            long bz = strtol(separator + 1, &end, 10);

            if (end == separator + 1 || end == NULL || *end != '\0') {
                continue;
            }

            if (fabs((double) bx - centerX) > 100.0 || fabs((double) bz - centerZ) > 100.0) {
                continue;
            }

            changedX[changeCount] = (int) bx;
            changedZ[changeCount] = (int) bz;
            changedType[changeCount] = (int) zval_get_long(changeValue);
            changeCount++;
        } ZEND_HASH_FOREACH_END();
    }

    zend_string *result = zend_string_alloc(NAUTIC_MAP_BUFFER_SIZE, 0);
    unsigned char *buffer = (unsigned char *) ZSTR_VAL(result);

    nautic_render_map_core(
        centerX,
        centerZ,
        yaw,
        (int64_t) worldSeed,
        (int) plotSize,
        (int) gap,
        (int) seaLevel,
        (int) oceanBase,
        (int) oceanVar,
        (int) islandHeightPeak,
        (int) beachWidth,
        (int) radMin,
        (int) radMax,
        otherCount,
        otherPlayers,
        changeCount,
        changedX,
        changedZ,
        changedType,
        buffer
    );

    if (otherPlayers != NULL) {
        efree(otherPlayers);
    }
    if (changedX != NULL) {
        efree(changedX);
        efree(changedZ);
        efree(changedType);
    }

    ZSTR_VAL(result)[NAUTIC_MAP_BUFFER_SIZE] = '\0';
    RETURN_STR(result);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_nautic_add,
    0,
    2,
    IS_LONG,
    0
)
    ZEND_ARG_TYPE_INFO(0, a, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, b, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_nautic_render_map,
    0,
    15,
    IS_STRING,
    0
)
    ZEND_ARG_TYPE_INFO(0, centerX, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, centerZ, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, yaw, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, worldSeed, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, plotSize, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, gap, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, seaLevel, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, oceanBase, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, oceanVar, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, islandHeightPeak, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, beachWidth, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, radMin, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, radMax, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, otherPlayers, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, changes, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry nauticrenderer_functions[] = {
    PHP_FE(nautic_add, arginfo_nautic_add)
    PHP_FE(nautic_render_map, arginfo_nautic_render_map)
    PHP_FE_END
};

PHP_MINFO_FUNCTION(nauticrenderer)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "NauticRenderer support", "enabled");
    php_info_print_table_row(2, "Renderer type", "native static C");
    php_info_print_table_row(2, "Map output", "128x128 RGB binary string");
    php_info_print_table_row(2, "Map buffer size", "49152 bytes");
    php_info_print_table_end();
}

zend_module_entry nauticrenderer_module_entry = {
    STANDARD_MODULE_HEADER,
    "nauticrenderer",
    nauticrenderer_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_MINFO(nauticrenderer),
    PHP_NAUTICRENDERER_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_NAUTICRENDERER
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(nauticrenderer)
#endif
