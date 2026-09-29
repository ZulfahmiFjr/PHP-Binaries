PHP_ARG_ENABLE(
  [nauticrenderer],
  [whether to enable Nautic native renderer],
  [AS_HELP_STRING(
    [--enable-nauticrenderer],
    [Enable Nautic native renderer]
  )],
  [no]
)

if test "$PHP_NAUTICRENDERER" != "no"; then
  PHP_NEW_EXTENSION(
    [nauticrenderer],
    [nauticrenderer.c],
    [$ext_shared],
    ,
    [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1]
  )
fi
