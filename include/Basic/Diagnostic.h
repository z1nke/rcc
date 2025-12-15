#ifndef RCC_BASIC_DIAGNOSTIC_H
#define RCC_BASIC_DIAGNOSTIC_H

#include "Basic/SourceLocation.h"

#include <cstdarg>

namespace rcc {

class SourceManager;
class Token;

class Diagnostic {
public:
  Diagnostic(SourceManager &SM) : SM(SM) {}

  [[noreturn]] void fatalAt(const char *Loc, const char *Fmt, ...);

  [[noreturn]] void fatalAt(SourceLocation Loc, const char *Fmt, ...);

  [[noreturn]] void fatal(const char *Fmt, ...);

  SourceManager &getSourceManager() const { return SM; }

private:
  [[noreturn]] void vfatal(const char *Loc, const char *Fmt, std::va_list AP);

private:
  SourceManager &SM;
};

} // namespace rcc

#endif