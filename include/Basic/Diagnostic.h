#ifndef RCC_BASIC_DIAGNOSTIC_H
#define RCC_BASIC_DIAGNOSTIC_H

#include "Basic/SourceManager.h"

#include <print>
#include <utility>

namespace rcc {

class Token;

class Diagnostic {
public:
  Diagnostic(SourceManager &SM) : SM(SM) {}

  template <typename... ARGS>
  [[noreturn]] void fatalAt(const char *Loc, std::format_string<ARGS...> Fmt,
                            ARGS &&...Args) const {
    vfatal(Loc, Fmt, std::forward<ARGS>(Args)...);
  }

  template <typename... ARGS>
  [[noreturn]] void fatalAt(SourceLocation SL, std::format_string<ARGS...> Fmt,
                            ARGS &&...Args) const {
    const char *Loc = SM.getLoc(SL);
    vfatal(Loc, Fmt, std::forward<ARGS>(Args)...);
  }

  template <typename... ARGS>
  [[noreturn]] void fatal(std::format_string<ARGS...> Fmt,
                          ARGS &&...Args) const {
    vfatal(nullptr, Fmt, std::forward<ARGS>(Args)...);
  }

  SourceManager &getSourceManager() const { return SM; }

private:
  template <typename... ARGS>
  [[noreturn]] void vfatal(const char *Loc, std::format_string<ARGS...> Fmt,
                           ARGS &&...Args) const {
    if (Loc)
      printLoc(Loc);
    printErrorWithColor();
    auto Msg = std::format(Fmt, std::forward<ARGS>(Args)...);
    std::println(stderr, "{}", Msg);

    std::exit(1);
  }

  void printLoc(const char *Loc) const;
  void printErrorWithColor() const;

private:
  SourceManager &SM;
};

} // namespace rcc

#endif