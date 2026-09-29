#pragma once
#include <limits>

namespace gstd {
// The original Windows/MSVC VM uses binary64 even though its source spells
// the type "long double". Wasm's binary128 long double is NOT that ABI: it
// changes expression rounding, variadic arguments, and random distributions.
// Keep the Windows source type unchanged; give the portable VM its actual
// original precision explicitly instead of changing the system C++ headers.
#ifdef __EMSCRIPTEN__
using script_real = double;
static_assert(sizeof(script_real) == 8 && std::numeric_limits<script_real>::digits == 53,
              "The Woo VM requires the Windows binary64 numeric contract");
#else
using script_real = long double;
#endif
}
