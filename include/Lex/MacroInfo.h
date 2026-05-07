#ifndef RCC_LEX_MACROINFO_H
#define RCC_LEX_MACROINFO_H

#include "Lex/Token.h"

#include <string>
#include <utility>
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

  void addParameter(std::string Name) {
    Parameters.push_back(std::move(Name));
  }
  const std::vector<std::string> &parameters() const { return Parameters; }

  bool isDisabled() const { return IsDisabled; }
  void disableMacro() { IsDisabled = true; }
  void enableMacro() { IsDisabled = false; }

private:
  std::vector<Token> ReplacementTokens;
  std::vector<std::string> Parameters;
  bool IsFunctionLike = false;
  bool IsDisabled = false;
};

} // namespace rcc

#endif
