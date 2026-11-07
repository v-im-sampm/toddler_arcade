// Little Arcade port shim: GCC builtins used by the vendored engine, for MSVC host builds.
#pragma once
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define __builtin_popcount(x) static_cast<int>(__popcnt(static_cast<unsigned>(x)))
#endif

