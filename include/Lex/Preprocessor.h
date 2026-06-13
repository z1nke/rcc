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
               const std::vector<CommandLineMacro> &CommandLineMacros,
               std::string BaseFile)
      : Diag(Diag), Lex(Lex), IncludePaths(IncludePaths),
        CommandLineMacros(CommandLineMacros), BaseFile(std::move(BaseFile)) {}

  Token *preprocess(Token *Toks);

  /// Search -I / system include dirs for \p Filename. Empty if not found.
  std::string searchIncludePaths(const std::string &Filename) const;

private:
  Token *joinAdjacentStringLiterals(Token *Toks);
  struct MacroExpansionFrame {
    MacroInfo *MI;
    Token *End;
  };

  std::int64_t evaluateDirectiveExpression(Token *&Rest, Token *Toks);
  void handleDefineDirective(Token *&Rest, Token *NameTok);
  void handleUndefDirective(Token *&Rest, Token *NameTok);
  void handleLineDirective(Token *&Rest, Token *Tok);
  void defineMacro(const char *Name, const char *Body);
  void undefMacro(const std::string &Name);
  void defineCommandLineMacro(const std::string &Def);
  void applyCommandLineMacros();
  void addBuiltin(const char *Name, BuiltinMacroFn Fn);
  void initMacros();
  Token *expandFileMacro(Token *Tmpl);
  Token *expandLineMacro(Token *Tmpl);
  Token *expandCounterMacro(Token *Tmpl);
  Token *expandTimestampMacro(Token *Tmpl);
  Token *expandBaseFileMacro(Token *Tmpl);
  static Token *handleFileMacro(Preprocessor &PP, Token *Tmpl);
  static Token *handleLineMacro(Preprocessor &PP, Token *Tmpl);
  static Token *handleCounterMacro(Preprocessor &PP, Token *Tmpl);
  static Token *handleTimestampMacro(Preprocessor &PP, Token *Tmpl);
  static Token *handleBaseFileMacro(Preprocessor &PP, Token *Tmpl);
  std::string readIncludeFilename(Token *&Rest, Token *Tok, bool &IsDquote);
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
  std::string BaseFile; // [GNU] main input path for __BASE_FILE__
  std::unordered_map<std::string, MacroInfo> Macros;
  std::vector<MacroExpansionFrame> MacroExpansionStack;
  BumpPtrAllocator MacroTokenAlloc;
  int Counter = 0; // [GNU] __COUNTER__
};

} // namespace rcc

#endif
