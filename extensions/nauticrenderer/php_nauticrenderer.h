#ifndef PHP_NAUTICRENDERER_H
#define PHP_NAUTICRENDERER_H

extern zend_module_entry nauticrenderer_module_entry;

#define phpext_nauticrenderer_ptr &nauticrenderer_module_entry
#define PHP_NAUTICRENDERER_VERSION "0.2.0"

#if defined(ZTS) && defined(COMPILE_DL_NAUTICRENDERER)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif
