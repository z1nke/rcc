#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"

#include <cassert>
#include <cstdlib>
#include <print>

#if _WIN32
#if __has_include(<io.h>)
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#define HAS_COLOR 1
#else
#define HAS_COLOR 0
#endif
#elif unix
#if __has_include(<unistd.h>)
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#define HAS_COLOR 1
#else
#define HAS_COLOR 0
#endif
#else
#define HAS_COLOR 0
#endif

namespace rcc {

void Diagnostic::printLoc(const char *Loc) const {
  const char *Start = SM.getBegin();
  std::println(stderr, "{}", Start);
  int Offset = Loc - SM.getBegin();
  std::print(stderr, "{:>{}}^ ", "", Offset);
}

void Diagnostic::printErrorWithColor() const {
#if HAS_COLOR
  const bool HasColor = ISATTY(FILENO(stderr));
  if (HasColor)
    std::print(stderr, "\033[0;1;31m");
#endif
  std::print(stderr, "error: ");
#if HAS_COLOR
  if (HasColor)
    std::print(stderr, "\033[0m");
#endif
}

} // namespace rcc
