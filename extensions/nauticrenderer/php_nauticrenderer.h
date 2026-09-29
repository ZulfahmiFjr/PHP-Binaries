#ifndef PHP_NAUTICRENDERER_H
#define PHP_NAUTICRENDERER_H

extern zend_module_entry nauticrenderer_module_entry;

#define phpext_nauticrenderer_ptr &nauticrenderer_module_entry

#define PHP_NAUTICRENDERER_VERSION "1.0.0"
#define PHP_NAUTICRENDERER_API_VERSION 1

#define NAUTIC_MAP_WIDTH 128
#define NAUTIC_MAP_HEIGHT 128
#define NAUTIC_MAP_CHANNELS 3
#define NAUTIC_MAP_BUFFER_SIZE (NAUTIC_MAP_WIDTH * NAUTIC_MAP_HEIGHT * NAUTIC_MAP_CHANNELS)

#if defined(ZTS) && defined(COMPILE_DL_NAUTICRENDERER)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif
