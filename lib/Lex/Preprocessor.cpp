#include "Lex/Preprocessor.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "Lex/Lexer.h"
#include "Lex/Token.h"

#include <filesystem>
#include <string_view>
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
    if (Toks->is(Token::TK_Hash) && Toks->isAtStartOfLine()) {
      Token *Start = Toks;
      Toks = Toks->getNext();
      if (Toks->is(Token::TK_EOF) || Toks->isAtStartOfLine())
        continue;

      bool ParentActive =
          ConditionalStack.empty() || ConditionalStack.back().Active;

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

      if (hasSpelling(Toks, "include")) {
        Token *FilenameTok = Toks->getNext();
        if (FilenameTok->isNot(Token::TK_StrLiteral))
          Diag.fatalAt(FilenameTok->getLoc(), "expected a filename");

        SourceManager &SM = Diag.getSourceManager();
        SourceLocation Loc = SM.createBeginLocation(FilenameTok);
        std::filesystem::path IncludingFile(SM.getFilename(Loc));
        std::filesystem::path IncludePath =
            IncludingFile.parent_path() / FilenameTok->getStringLiteral(Diag);

        Token *Included = Lex.tokenizeFile(IncludePath.c_str());
        Token *Rest = skipLine(FilenameTok->getNext(), Diag);
        Toks = append(Included, Rest);
        continue;
      }

      Diag.fatalAt(Toks->getLoc(), "invalid preprocessor directive");
    }

    if (!ConditionalStack.empty() && !ConditionalStack.back().Active) {
      Toks = Toks->getNext();
      continue;
    }

    Curr->setNext(Toks);
    Curr = Toks;
    Toks = Toks->getNext();
  }

  if (!ConditionalStack.empty())
    Diag.fatalAt(ConditionalStack.back().Start->getLoc(),
                 "unterminated conditional directive");

  Curr->setNext(Toks);
  return Head.getNext();
}

} // namespace rcc
