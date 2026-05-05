#ifndef RCC_LEX_PREPROCESSOR_H
#define RCC_LEX_PREPROCESSOR_H

#include "Lex/MacroInfo.h"
#include "Support/Allocator.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace rcc {

class Diagnostic;
class Lexer;
class Token;

class Preprocessor {
public:
  Preprocessor(Diagnostic &Diag, Lexer &Lex) : Diag(Diag), Lex(Lex) {}

  Token *preprocess(Token *Toks);

private:
  std::int64_t evaluateDirectiveExpression(Token *&Rest, Token *Toks);
  void handleDefineDirective(Token *&Rest, Token *NameTok);
  bool expandMacro(Token *&Rest, Token *Tok);
  static bool isMacroIdentifier(const Token *Tok);

  Diagnostic &Diag;
  Lexer &Lex;
  std::unordered_map<std::string, MacroInfo> Macros;
  BumpPtrAllocator MacroTokenAlloc;
};

} // namespace rcc

#endif
