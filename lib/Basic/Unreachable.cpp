#include "Basic/Unreachable.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace rcc {
namespace details {

#if defined __has_builtin
#if __has_builtin(__builtin_unreachable)
#define RCC_BUILTIN_UNREACHABLE __builtin_unreachable()
#endif
#endif

#if !defined(RCC_BUILTIN_UNREACHABLE) && defined(_MSC_VER)
#define RCC_BUILTIN_UNREACHABLE __assume(false)
#endif

#ifndef RCC_BUILTIN_UNREACHABLE
#define RCC_BUILTIN_UNREACHABLE
#endif

[[noreturn]] void unreachableInternal(const char *msg, const char *filename,
                                      unsigned lineno) {
#ifndef NDEBUG // DEBUG
  if (msg)
    std::fprintf(stderr, "%s\n", msg);
  std::fputs("Unreachable executed", stderr);
  if (filename)
    std::fprintf(stderr, " at %s:%u", filename, lineno);
  std::fputs("!\n", stderr);
#endif
  std::abort();
  RCC_BUILTIN_UNREACHABLE;
}

} // namespace details
} // namespace rcc