#ifndef RCC_BASIC_DIAGNOSTIC_H
#define RCC_BASIC_DIAGNOSTIC_H

#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"
#include "Support/Error.h"

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
    SourceLocation SL = SM.createBeginLocation(Loc);
    vfatal(SL, Fmt, std::forward<ARGS>(Args)...);
  }

  template <typename... ARGS>
  [[noreturn]] void fatalAt(SourceLocation SL, std::format_string<ARGS...> Fmt,
                            ARGS &&...Args) const {
    vfatal(SL, Fmt, std::forward<ARGS>(Args)...);
  }

  template <typename... ARGS>
  void warnAt(const char *Loc, std::format_string<ARGS...> Fmt,
              ARGS &&...Args) const {
    SourceLocation SL = SM.createBeginLocation(Loc);
    vwarn(SL, Fmt, std::forward<ARGS>(Args)...);
  }

  template <typename... ARGS>
  [[noreturn]] void fatal(std::format_string<ARGS...> Fmt,
                          ARGS &&...Args) const {
    vfatal(SourceLocation(), Fmt, std::forward<ARGS>(Args)...);
  }

  SourceManager &getSourceManager() const { return SM; }

private:
  template <typename... ARGS>
  [[noreturn]] void vfatal(SourceLocation Loc, std::format_string<ARGS...> Fmt,
                           ARGS &&...Args) const {
    printLoc(Loc);
    printErrorWithColor();
    auto Msg = std::format(Fmt, std::forward<ARGS>(Args)...);
    std::println(stderr, "{}", Msg);

    std::exit(1);
  }

  template <typename... ARGS>
  void vwarn(SourceLocation Loc, std::format_string<ARGS...> Fmt,
             ARGS &&...Args) const {
    printLoc(Loc);
    auto Msg = std::format(Fmt, std::forward<ARGS>(Args)...);
    std::println(stderr, "warning: {}", Msg);
  }

  void printLoc(SourceLocation Loc) const;

private:
  SourceManager &SM;
};

} // namespace rcc

#endif