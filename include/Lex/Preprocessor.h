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
  Preprocessor(Diagnostic &Diag, Lexer &Lex,
               const std::vector<std::string> &IncludePaths,
               const std::vector<CommandLineMacro> &CommandLineMacros)
      : Diag(Diag), Lex(Lex), IncludePaths(IncludePaths),
        CommandLineMacros(CommandLineMacros) {}

  Token *preprocess(Token *Toks);

private:
  Token *joinAdjacentStringLiterals(Token *Toks);
  struct MacroExpansionFrame {
    MacroInfo *MI;
    Token *End;
  };

  std::int64_t evaluateDirectiveExpression(Token *&Rest, Token *Toks);
  void handleDefineDirective(Token *&Rest, Token *NameTok);
  void handleUndefDirective(Token *&Rest, Token *NameTok);
  void defineMacro(const char *Name, const char *Body);
  void undefMacro(const std::string &Name);
  void defineCommandLineMacro(const std::string &Def);
  void applyCommandLineMacros();
  void addBuiltin(const char *Name, BuiltinMacroFn Fn);
  void initMacros();
  Token *expandFileMacro(Token *Tmpl);
  Token *expandLineMacro(Token *Tmpl);
  static Token *handleFileMacro(Preprocessor &PP, Token *Tmpl);
  static Token *handleLineMacro(Preprocessor &PP, Token *Tmpl);
  std::string readIncludeFilename(Token *&Rest, Token *Tok, bool &IsDquote);
  std::string searchIncludePaths(const std::string &Filename) const;
  Token *includeFile(Token *Rest, const std::string &Path, Token *FilenameTok);
  bool expandMacro(Token *&Rest, Token *Tok);
  Token *expandMacroExpression(Token *&Rest, Token *Toks);
  std::vector<const Token *>
  expandMacroArgument(const std::vector<const Token *> &Argument);
  void finishMacroExpansions(Token *Tok);
  static bool isMacroIdentifier(const Token *Tok);

  Diagnostic &Diag;
  Lexer &Lex;
  const std::vector<std::string> &IncludePaths;
  const std::vector<CommandLineMacro> &CommandLineMacros;
  std::unordered_map<std::string, MacroInfo> Macros;
  std::vector<MacroExpansionFrame> MacroExpansionStack;
  BumpPtrAllocator MacroTokenAlloc;
};

} // namespace rcc

#endif
