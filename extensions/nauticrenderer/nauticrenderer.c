#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_nauticrenderer.h"

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

static const zend_function_entry nauticrenderer_functions[] = {
    PHP_FE(nautic_add, arginfo_nautic_add)
    PHP_FE_END
};

PHP_MINFO_FUNCTION(nauticrenderer)
{
    php_info_print_table_start();
    php_info_print_table_row(
        2,
        "Nautic Renderer support",
        "enabled"
    );
    php_info_print_table_row(
        2,
        "Renderer type",
        "native static C"
    );
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
