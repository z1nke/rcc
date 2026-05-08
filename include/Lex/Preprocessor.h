#ifndef RCC_LEX_PREPROCESSOR_H
#define RCC_LEX_PREPROCESSOR_H

#include "Lex/MacroInfo.h"
#include "Support/Allocator.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rcc {

class Diagnostic;
class Lexer;
class Token;

class Preprocessor {
public:
  Preprocessor(Diagnostic &Diag, Lexer &Lex) : Diag(Diag), Lex(Lex) {}

  Token *preprocess(Token *Toks);

private:
  struct MacroExpansionFrame {
    MacroInfo *MI;
    Token *End;
  };

  std::int64_t evaluateDirectiveExpression(Token *&Rest, Token *Toks);
  void handleDefineDirective(Token *&Rest, Token *NameTok);
  void handleUndefDirective(Token *&Rest, Token *NameTok);
  bool expandMacro(Token *&Rest, Token *Tok);
  Token *expandMacroExpression(Token *&Rest, Token *Toks);
  std::vector<const Token *>
  expandMacroArgument(const std::vector<const Token *> &Argument);
  void finishMacroExpansions(Token *Tok);
  static bool isMacroIdentifier(const Token *Tok);

  Diagnostic &Diag;
  Lexer &Lex;
  std::unordered_map<std::string, MacroInfo> Macros;
  std::vector<MacroExpansionFrame> MacroExpansionStack;
  BumpPtrAllocator MacroTokenAlloc;
};

} // namespace rcc

#endif
