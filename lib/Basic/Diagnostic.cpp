#include "Basic/Diagnostic.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

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

[[noreturn]] void Diagnostic::fatalAt(const char *Loc, const char *Fmt, ...) {
  const char *Start = SM.getStart();
  fprintf(stderr, "%s\n", Start);
  int Offset = Loc - SM.getStart();
  fprintf(stderr, "%*s", Offset, "^ ");
  va_list AP;
  va_start(AP, Fmt);
  vfatal(Loc, Fmt, AP);
}

[[noreturn]] void Diagnostic::Diagnostic::fatal(const char *Fmt, ...) {
  va_list AP;
  va_start(AP, Fmt);
  vfatal(nullptr, Fmt, AP);
}

[[noreturn]] void Diagnostic::vfatal(const char *Loc, const char *Fmt,
                                     std::va_list AP) {
#if HAS_COLOR
  const bool HasColor = ISATTY(FILENO(stderr));
  if (HasColor)
    fprintf(stderr, "\033[0;1;31m");
#endif
  fprintf(stderr, "error: ");
#if HAS_COLOR
  if (HasColor)
    fprintf(stderr, "\033[0m");
#endif
  vfprintf(stderr, Fmt, AP);
  fputc('\n', stderr);
  va_end(AP);
  std::exit(1);
}

} // namespace rcc
