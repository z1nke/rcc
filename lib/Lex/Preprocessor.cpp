#include "Lex/Preprocessor.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "Lex/Lexer.h"
#include "Lex/Token.h"

#include <cstdio>
#include <cstring>
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
  initMacros();

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
          Diag.fatalAt(NameTok->getLoc(), "macro name must be an identifier");

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
        std::string Filename = readIncludeFilename(Toks, FilenameTok, IsDquote);

        // Quoted includes are first resolved relative to the including file.
        if (!Filename.empty() && Filename[0] != '/' && IsDquote) {
          SourceManager &SM = Diag.getSourceManager();
          SourceLocation Loc = SM.createBeginLocation(Start);
          // Use the real path, not a #line remapped display name.
          std::filesystem::path IncludingFile(SM.getFileEntry(Loc)->getPath());
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

      if (hasSpelling(Toks, "line")) {
        handleLineDirective(Toks, Toks->getNext());
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
  Lex.convertPPTokens(Head.getNext());
  return joinAdjacentStringLiterals(Head.getNext());
}

Token *Preprocessor::joinAdjacentStringLiterals(Token *Toks) {
  Token Head;
  Head.setNext(Toks);

  for (Token *Prev = &Head; Prev->getNext()->isNot(Token::TK_EOF);) {
    Token *Tok = Prev->getNext();
    if (Tok->isNot(Token::TK_StrLiteral) ||
        Tok->getNext()->isNot(Token::TK_StrLiteral)) {
      Prev = Tok;
      continue;
    }

    Token *End = Tok->getNext();
    while (End->is(Token::TK_StrLiteral))
      End = End->getNext();

    // Resolve the common encoding for this adjacent run. A bare / u8 string
    // may be concatenated with one of L/u/U; mixing distinct L/u/U is an error.
    using Kind = Token::StringLiteralKind;
    Kind ResultKind = Kind::Narrow;
    bool HasWideKind = false;
    for (Token *T = Tok; T != End; T = T->getNext()) {
      Kind K = T->getStringLiteralKind();
      if (K == Kind::Narrow || K == Kind::UTF8)
        continue;
      if (!HasWideKind) {
        ResultKind = K;
        HasWideKind = true;
      } else if (ResultKind != K) {
        Diag.fatalAt(T->getLoc(),
                     "unsupported non-standard concatenation of string "
                     "literals");
      }
    }

    std::string Content;
    for (Token *T = Tok; T != End; T = T->getNext())
      Content += T->getStringLiteralAs(Diag, ResultKind);

    std::string Spelling;
    if (ResultKind == Kind::Wide)
      Spelling = "L\"\"";
    else if (ResultKind == Kind::UTF16)
      Spelling = "u\"\"";
    else if (ResultKind == Kind::UTF32)
      Spelling = "U\"\"";
    else {
      Spelling = "\"";
      for (unsigned char C : Content) {
        switch (C) {
        case '\a':
          Spelling += "\\a";
          break;
        case '\b':
          Spelling += "\\b";
          break;
        case '\f':
          Spelling += "\\f";
          break;
        case '\n':
          Spelling += "\\n";
          break;
        case '\r':
          Spelling += "\\r";
          break;
        case '\t':
          Spelling += "\\t";
          break;
        case '\v':
          Spelling += "\\v";
          break;
        case '\\':
          Spelling += "\\\\";
          break;
        case '"':
          Spelling += "\\\"";
          break;
        default:
          if (C >= 0x20 && C < 0x7f) {
            Spelling += static_cast<char>(C);
          } else {
            // Use a fixed-width octal escape so the next character cannot
            // extend it (e.g. "\x9" "0" must stay "\t0", not "\x90").
            char Buf[8];
            std::snprintf(Buf, sizeof(Buf), "\\%03o", C);
            Spelling += Buf;
          }
          break;
        }
      }
      Spelling += '"';
    }

    char *Buffer = static_cast<char *>(
        MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
    std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Joined =
        new (Mem) Token(Token::TK_StrLiteral, Buffer, Buffer + Spelling.size());
    Joined->setAtStartOfLine(Tok->isAtStartOfLine());
    Joined->setHasLeadingSpace(Tok->hasLeadingSpace());
    Joined->setSourceRange(*Tok);

    if (HasWideKind) {
      char *Data = static_cast<char *>(
          MacroTokenAlloc.allocate(Content.size() + 1, alignof(char)));
      std::memcpy(Data, Content.data(), Content.size());
      Data[Content.size()] = '\0';
      Joined->setStringLiteralData(Data, static_cast<int>(Content.size()));
    }

    Joined->setNext(End);
    Prev->setNext(Joined);
    Prev = Joined;
  }
  return Head.getNext();
}

} // namespace rcc
