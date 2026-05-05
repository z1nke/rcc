#include "Lex/Preprocessor.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "Lex/Lexer.h"
#include "Lex/Token.h"

#include <filesystem>

namespace rcc {

static Token *skipLine(Token *Toks, Diagnostic &Diag) {
  if (Toks->is(Token::TK_EOF) || Toks->isAtStartOfLine())
    return Toks;

  Diag.warnAt(Toks->getLoc(), "extra token at end of #include directive");
  do {
    Toks = Toks->getNext();
  } while (Toks->isNot(Token::TK_EOF) && !Toks->isAtStartOfLine());
  return Toks;
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
  Token Head;
  Token *Curr = &Head;

  while (Toks->isNot(Token::TK_EOF)) {
    if (Toks->is(Token::TK_Hash) && Toks->isAtStartOfLine()) {
      Toks = Toks->getNext();
      if (Toks->is(Token::TK_EOF) || Toks->isAtStartOfLine())
        continue;

      if (Toks->is(Token::TK_Ident) && Toks->getIdentifer() == "include") {
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

    Curr->setNext(Toks);
    Curr = Toks;
    Toks = Toks->getNext();
  }

  Curr->setNext(Toks);
  return Head.getNext();
}

} // namespace rcc
