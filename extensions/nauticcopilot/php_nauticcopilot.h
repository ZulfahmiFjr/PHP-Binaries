#ifndef PHP_NAUTICCOPILOT_H
#define PHP_NAUTICCOPILOT_H

#include "php.h"

extern zend_module_entry nauticcopilot_module_entry;
#define phpext_nauticcopilot_ptr &nauticcopilot_module_entry

#define PHP_NAUTICCOPILOT_VERSION "1.0.0"
#define NAUTIC_COPILOT_API_VERSION 1

#if defined(ZTS) && defined(COMPILE_DL_NAUTICCOPILOT)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif
