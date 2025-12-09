#ifndef RCC_BASIC_DIAGNOSTIC_H
#define RCC_BASIC_DIAGNOSTIC_H

#include <cstdarg>

namespace rcc {

class SourceManager;
class Token;

class Diagnostic {
public:
  Diagnostic(SourceManager &SM) : SM(SM) {}

  [[noreturn]] void fatalAt(const char *Loc, const char *Fmt, ...);

  [[noreturn]] static void fatal(const char *Fmt, ...);

  SourceManager &getSourceManager() const { return SM; }

private:
  [[noreturn]] static void vfatal(const char *Loc, const char *Fmt,
                                  std::va_list AP);

private:
  SourceManager &SM;
};

} // namespace rcc

#endif