PHP_ARG_ENABLE([atlasrenderer], [whether to enable AtlasBoard renderer],
  [AS_HELP_STRING([--enable-atlasrenderer], [Enable AtlasBoard C++ renderer])], [no])
if test "$PHP_ATLASRENDERER" != "no"; then
  PHP_REQUIRE_CXX()
  PHP_NEW_EXTENSION(atlasrenderer, atlasrenderer.cpp renderer.cpp, $ext_shared,, -std=c++17 -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1, yes)
  PHP_ADD_LIBRARY(stdc++, 1, ATLASRENDERER_SHARED_LIBADD)
  PHP_SUBST(ATLASRENDERER_SHARED_LIBADD)
fi
