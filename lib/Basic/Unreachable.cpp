#include "Basic/Unreachable.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <print>

#include <print>

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

[[noreturn]] void unreachableInternal(const char *Msg, const char *Filename,
                                      unsigned Lineno) {
#ifndef NDEBUG // DEBUG
  if (Msg)
    std::println(stderr, "{}", Msg);
  std::fputs("Unreachable executed", stderr);
  if (Filename)
    std::print(stderr, " at {}:{}", Filename, Lineno);
  std::fputs("!\n", stderr);
#endif
  std::abort();
  RCC_BUILTIN_UNREACHABLE;
}

} // namespace details
} // namespace rcc