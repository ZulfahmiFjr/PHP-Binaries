PHP_ARG_ENABLE([nauticcopilot], [whether to enable NauticCraft copilot acceleration],
  [AS_HELP_STRING([--enable-nauticcopilot], [Enable NauticCraft C++ copilot extension])], [no])

if test "$PHP_NAUTICCOPILOT" != "no"; then
  PHP_REQUIRE_CXX()
  PHP_NEW_EXTENSION(nauticcopilot, nauticcopilot.cpp copilot.cpp, $ext_shared,, -std=c++17 -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1, yes)
  PHP_ADD_LIBRARY(stdc++, 1, NAUTICCOPILOT_SHARED_LIBADD)
  PHP_SUBST(NAUTICCOPILOT_SHARED_LIBADD)
fi
