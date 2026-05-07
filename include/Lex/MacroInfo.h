#ifndef RCC_LEX_MACROINFO_H
#define RCC_LEX_MACROINFO_H

#include "Lex/Token.h"

#include <vector>

namespace rcc {

class MacroInfo {
public:
  void addTokenToBody(const Token &Tok) {
    ReplacementTokens.push_back(Tok);
    ReplacementTokens.back().setNext(nullptr);
  }

  const std::vector<Token> &tokens() const { return ReplacementTokens; }

  bool isFunctionLike() const { return IsFunctionLike; }
  void setIsFunctionLike() { IsFunctionLike = true; }

  bool isDisabled() const { return IsDisabled; }
  void disableMacro() { IsDisabled = true; }
  void enableMacro() { IsDisabled = false; }

private:
  std::vector<Token> ReplacementTokens;
  bool IsFunctionLike = false;
  bool IsDisabled = false;
};

} // namespace rcc

#endif
