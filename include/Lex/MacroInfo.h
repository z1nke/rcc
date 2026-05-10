#ifndef RCC_LEX_MACROINFO_H
#define RCC_LEX_MACROINFO_H

#include "Lex/Token.h"

#include <string>
#include <utility>
#include <vector>

namespace rcc {

class Preprocessor;

// Builtin macro expander, e.g. for __FILE__ / __LINE__.
using BuiltinMacroFn = Token *(*)(Preprocessor &PP, Token *Tmpl);

class MacroInfo {
public:
  void addTokenToBody(const Token &Tok) {
    ReplacementTokens.push_back(Tok);
    ReplacementTokens.back().setNext(nullptr);
  }

  const std::vector<Token> &tokens() const { return ReplacementTokens; }

  bool isFunctionLike() const { return IsFunctionLike; }
  void setIsFunctionLike() { IsFunctionLike = true; }

  void addParameter(std::string Name) {
    Parameters.push_back(std::move(Name));
  }
  const std::vector<std::string> &parameters() const { return Parameters; }

  bool isVariadic() const { return IsVariadic; }
  void setIsVariadic() { IsVariadic = true; }

  bool isDisabled() const { return IsDisabled; }
  void disableMacro() { IsDisabled = true; }
  void enableMacro() { IsDisabled = false; }

  void setHandler(BuiltinMacroFn Fn) { Handler = Fn; }
  BuiltinMacroFn getHandler() const { return Handler; }
  bool hasHandler() const { return Handler != nullptr; }

private:
  std::vector<Token> ReplacementTokens;
  std::vector<std::string> Parameters;
  BuiltinMacroFn Handler = nullptr;
  bool IsFunctionLike = false;
  bool IsVariadic = false;
  bool IsDisabled = false;
};

/// A command-line -D or -U macro action, applied in order after builtins.
struct CommandLineMacro {
  enum Kind { Define, Undef } Action = Define;
  std::string Text; // Define: "name" or "name=value"; Undef: "name"
};

} // namespace rcc

#endif
