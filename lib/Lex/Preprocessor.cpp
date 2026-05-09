#include "Lex/Preprocessor.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "Lex/Lexer.h"
#include "Lex/Token.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace rcc {

static bool hasSpelling(const Token *Tok, std::string_view Spelling) {
  return std::string_view(Tok->getLoc(), Tok->getLen()) == Spelling;
}

static Token *skipToNextLine(Token *Toks) {
  while (Toks->isNot(Token::TK_EOF) && !Toks->isAtStartOfLine())
    Toks = Toks->getNext();
  return Toks;
}

static Token *skipLine(Token *Toks, Diagnostic &Diag) {
  if (Toks->is(Token::TK_EOF) || Toks->isAtStartOfLine())
    return Toks;

  Diag.warnAt(Toks->getLoc(), "extra token");
  return skipToNextLine(Toks);
}

static Token *append(Token *Included, Token *Rest) {
  if (Included->is(Token::TK_EOF))
    return Rest;

  Token *Last = Included;
  while (Last->getNext()->isNot(Token::TK_EOF))
    Last = Last->getNext();
  Last->setNext(Rest);
  return Included;
}

static std::string joinTokens(Token *Tok, Token *End) {
  std::string Buf;
  for (Token *T = Tok; T != End && T->isNot(Token::TK_EOF); T = T->getNext()) {
    if (T != Tok && T->hasLeadingSpace())
      Buf += ' ';
    Buf.append(T->getLoc(), T->getLen());
  }
  return Buf;
}

static bool fileExists(const std::filesystem::path &Path) {
  std::error_code EC;
  return std::filesystem::exists(Path, EC) && !EC;
}

std::string Preprocessor::readIncludeFilename(Token *&Rest, Token *Tok,
                                              bool &IsDquote) {
  // #include "foo.h"
  if (Tok->is(Token::TK_StrLiteral)) {
    // Include filenames are not escape-processed.
    IsDquote = true;
    Rest = skipLine(Tok->getNext(), Diag);
    return std::string(Tok->getLoc() + 1, Tok->getLen() - 2);
  }

  // #include <foo.h>
  if (Tok->is(Token::TK_Less)) {
    Token *Start = Tok;
    for (; Tok->isNot(Token::TK_Greater); Tok = Tok->getNext()) {
      if (Tok->isAtStartOfLine() || Tok->is(Token::TK_EOF))
        Diag.fatalAt(Tok->getLoc(), "expected '>'");
    }

    IsDquote = false;
    Rest = skipLine(Tok->getNext(), Diag);
    return joinTokens(Start->getNext(), Tok);
  }

  // #include FOO — FOO must expand to a string or a <...> sequence.
  if (isMacroIdentifier(Tok)) {
    Token *Expanded = expandMacroExpression(Rest, Tok);
    return readIncludeFilename(Expanded, Expanded, IsDquote);
  }

  Diag.fatalAt(Tok->getLoc(), "expected a filename");
}

std::string
Preprocessor::searchIncludePaths(const std::string &Filename) const {
  if (!Filename.empty() && Filename[0] == '/')
    return Filename;

  for (const std::string &Dir : IncludePaths) {
    std::filesystem::path Path = std::filesystem::path(Dir) / Filename;
    if (fileExists(Path))
      return Path.string();
  }
  return {};
}

Token *Preprocessor::includeFile(Token *Rest, const std::string &Path,
                                 Token *FilenameTok) {
  std::error_code EC;
  if (!std::filesystem::exists(Path, EC) || EC)
    Diag.fatalAt(FilenameTok->getLoc(), "{}: cannot open file", Path);

  Token *Included = Lex.tokenizeFile(Path.c_str());
  return append(Included, Rest);
}

Token *Preprocessor::preprocess(Token *Toks) {
  struct ConditionalFrame {
    Token *Start;
    bool ParentActive;
    bool Active;
    bool BranchTaken;
    bool HasElse;
  };

  Token Head;
  Token *Curr = &Head;
  std::vector<ConditionalFrame> ConditionalStack;

  while (Toks->isNot(Token::TK_EOF)) {
    finishMacroExpansions(Toks);

    if (Toks->is(Token::TK_Hash) && Toks->isAtStartOfLine()) {
      Token *Start = Toks;
      Toks = Toks->getNext();
      if (Toks->is(Token::TK_EOF) || Toks->isAtStartOfLine())
        continue;

      bool ParentActive =
          ConditionalStack.empty() || ConditionalStack.back().Active;

      if (hasSpelling(Toks, "ifdef") || hasSpelling(Toks, "ifndef")) {
        bool IsIfndef = hasSpelling(Toks, "ifndef");
        Token *NameTok = Toks->getNext();
        if (!isMacroIdentifier(NameTok))
          Diag.fatalAt(NameTok->getLoc(),
                       "macro name must be an identifier");

        std::string Name(NameTok->getLoc(), NameTok->getLen());
        bool Condition = Macros.contains(Name) != IsIfndef;
        bool Active = ParentActive && Condition;
        ConditionalStack.push_back(
            ConditionalFrame{Start, ParentActive, Active, Active, false});
        Toks = skipLine(NameTok->getNext(), Diag);
        continue;
      }

      if (hasSpelling(Toks, "if")) {
        Token *Rest;
        bool Condition = false;
        if (ParentActive)
          Condition = evaluateDirectiveExpression(Rest, Toks->getNext()) != 0;
        else
          Rest = skipToNextLine(Toks->getNext());

        bool Active = ParentActive && Condition;
        ConditionalStack.push_back(
            ConditionalFrame{Start, ParentActive, Active, Active, false});
        Toks = Rest;
        continue;
      }

      if (hasSpelling(Toks, "elif")) {
        if (ConditionalStack.empty() || ConditionalStack.back().HasElse)
          Diag.fatalAt(Start->getLoc(), "stray #elif");

        ConditionalFrame &Frame = ConditionalStack.back();
        Token *Rest;
        bool Condition = false;
        if (Frame.ParentActive && !Frame.BranchTaken)
          Condition = evaluateDirectiveExpression(Rest, Toks->getNext()) != 0;
        else
          Rest = skipToNextLine(Toks->getNext());

        Frame.Active = Frame.ParentActive && !Frame.BranchTaken && Condition;
        Frame.BranchTaken = Frame.BranchTaken || Frame.Active;
        Toks = Rest;
        continue;
      }

      if (hasSpelling(Toks, "else")) {
        if (ConditionalStack.empty() || ConditionalStack.back().HasElse)
          Diag.fatalAt(Start->getLoc(), "stray #else");

        ConditionalFrame &Frame = ConditionalStack.back();
        Frame.HasElse = true;
        Frame.Active = Frame.ParentActive && !Frame.BranchTaken;
        Frame.BranchTaken = true;
        Toks = skipLine(Toks->getNext(), Diag);
        continue;
      }

      if (hasSpelling(Toks, "endif")) {
        if (ConditionalStack.empty())
          Diag.fatalAt(Start->getLoc(), "stray #endif");

        ConditionalStack.pop_back();
        Toks = skipLine(Toks->getNext(), Diag);
        continue;
      }

      if (!ParentActive) {
        Toks = skipToNextLine(Toks->getNext());
        continue;
      }

      if (hasSpelling(Toks, "define")) {
        Token *Rest;
        handleDefineDirective(Rest, Toks->getNext());
        Toks = Rest;
        continue;
      }

      if (hasSpelling(Toks, "undef")) {
        Token *Rest;
        handleUndefDirective(Rest, Toks->getNext());
        Toks = Rest;
        continue;
      }

      if (hasSpelling(Toks, "include")) {
        bool IsDquote = false;
        Token *FilenameTok = Toks->getNext();
        std::string Filename =
            readIncludeFilename(Toks, FilenameTok, IsDquote);

        // Quoted includes are first resolved relative to the including file.
        if (!Filename.empty() && Filename[0] != '/' && IsDquote) {
          SourceManager &SM = Diag.getSourceManager();
          SourceLocation Loc = SM.createBeginLocation(Start);
          std::filesystem::path IncludingFile(SM.getFilename(Loc));
          std::filesystem::path RelativePath =
              IncludingFile.parent_path() / Filename;
          if (fileExists(RelativePath)) {
            Toks = includeFile(Toks, RelativePath.string(), FilenameTok);
            continue;
          }
        }

        std::string Path = searchIncludePaths(Filename);
        Toks = includeFile(Toks, Path.empty() ? Filename : Path, FilenameTok);
        continue;
      }

      if (hasSpelling(Toks, "error"))
        Diag.fatalAt(Toks->getLoc(), "error");

      Diag.fatalAt(Toks->getLoc(), "invalid preprocessor directive");
    }

    if (!ConditionalStack.empty() && !ConditionalStack.back().Active) {
      Toks = Toks->getNext();
      continue;
    }

    Token *Tok = Toks;
    if (expandMacro(Toks, Tok))
      continue;

    Curr->setNext(Toks);
    Curr = Toks;
    Toks = Toks->getNext();
  }
  finishMacroExpansions(Toks);

  if (!ConditionalStack.empty())
    Diag.fatalAt(ConditionalStack.back().Start->getLoc(),
                 "unterminated conditional directive");

  Curr->setNext(Toks);
  return Head.getNext();
}

} // namespace rcc
