PHP_ARG_ENABLE([nauticrenderer],
  [whether to enable NauticRenderer support],
  [AS_HELP_STRING(
    [--enable-nauticrenderer],
    [Enable Nautic native renderer]
  )],
  [no]
)

if test "$PHP_NAUTICRENDERER" != "no"; then
  AC_DEFINE(
    [HAVE_NAUTICRENDERER],
    [1],
    [Have NauticRenderer support]
  )

  PHP_NEW_EXTENSION(
    nauticrenderer,
    nauticrenderer.c nautic_math.c,
    $ext_shared
  )
fi
