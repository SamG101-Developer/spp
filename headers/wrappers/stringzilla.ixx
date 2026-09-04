module;
// StringZilla includes <stdio.h> and <stdlib.h> from inside its own
// `extern "C"` block, which gives every libstdc++ declaration they
// reach C language linkage. Pulling them in first makes those inner
// includes no-ops, so std:: keeps C++ linkage and does not clash with
// the other header-wrapper modules.
#include <cstdio>
#include <cstdlib>
#include <stringzilla/stringzilla.hpp>

export module stringzilla;

export namespace stringzilla {
  using ::ashvardanian::stringzilla::string;
  using ::ashvardanian::stringzilla::string_view;
}
