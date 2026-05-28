#include "Basic/Diagnostic.h"
#include "Basic/FileEntry.h"
#include "Basic/SourceManager.h"
#include "Lex/Lexer.h"
#include "Lex/MacroInfo.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"

#include <cstring>
#include <ctime>
#include <new>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace rcc {

static std::size_t getParameterIndex(const MacroInfo &MI, const Token &Tok) {
  const auto &Parameters = MI.parameters();
  std::string_view Spelling(Tok.getLoc(), Tok.getLen());
  for (std::size_t I = 0; I < Parameters.size(); ++I)
    if (Parameters[I] == Spelling)
      return I;
  return Parameters.size();
}

static void
copyOperandTokens(BumpPtrAllocator &Alloc, const MacroInfo &MI,
                  const std::vector<std::vector<const Token *>> &Arguments,
                  const Token &Operand, Token *MacroNameTok,
                  std::vector<Token *> &Result) {
  std::size_t Index = getParameterIndex(MI, Operand);
  if (Index == MI.parameters().size()) {
    void *Mem = Alloc.allocate(sizeof(Token), alignof(Token));
    Result.push_back(new (Mem) Token(Operand));
    Result.back()->setNext(nullptr);
    Result.back()->setSourceRange(*MacroNameTok);
    Result.back()->setOrigin(MacroNameTok);
    return;
  }

  for (const Token *Tok : Arguments[Index]) {
    void *Mem = Alloc.allocate(sizeof(Token), alignof(Token));
    Result.push_back(new (Mem) Token(*Tok));
    Result.back()->setNext(nullptr);
    Result.back()->setOrigin(MacroNameTok);
  }
}

static void pasteTokens(BumpPtrAllocator &Alloc, Lexer &Lex, Diagnostic &Diag,
                        Token &LHS, const Token &RHS, const Token &PasteOp) {
  std::string Spelling(LHS.getLoc(), LHS.getLen());
  Spelling.append(RHS.getLoc(), RHS.getLen());
  bool HasLeadingSpace = LHS.hasLeadingSpace();
  Token SourceToken = LHS;

  char *Buffer =
      static_cast<char *>(Alloc.allocate(Spelling.size() + 1, alignof(char)));
  std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

  Token *Lexed = Lex.tokenize(Buffer);
  if (Lexed->getNext()->isNot(Token::TK_EOF))
    Diag.fatalAt(PasteOp.getLoc(),
                 "pasting forms '{}', an invalid preprocessing token",
                 Spelling);

  LHS = *Lexed;
  LHS.setNext(nullptr);
  LHS.setSourceRange(SourceToken);
  LHS.setHasLeadingSpace(HasLeadingSpace);
}

std::vector<const Token *>
Preprocessor::expandMacroArgument(const std::vector<const Token *> &Argument) {
  Token Head;
  Token *Curr = &Head;
  for (const Token *Tok : Argument) {
    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Curr->setNext(new (Mem) Token(*Tok));
    Curr = Curr->getNext();
    Curr->setNext(nullptr);
  }

  static const char Empty = '\0';
  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *EOFToken = new (Mem) Token(Token::TK_EOF, &Empty, &Empty);
  Curr->setNext(EOFToken);

  std::vector<const Token *> Result;
  Token *Toks = Head.getNext();
  while (Toks->isNot(Token::TK_EOF)) {
    finishMacroExpansions(Toks);
    Token *Tok = Toks;
    if (expandMacro(Toks, Tok))
      continue;

    Result.push_back(Toks);
    Toks = Toks->getNext();
  }
  finishMacroExpansions(Toks);
  return Result;
}

bool Preprocessor::expandMacro(Token *&Rest, Token *Tok) {
  if (!isMacroIdentifier(Tok) || Tok->isExpandDisabled())
    return false;

  std::string Name(Tok->getLoc(), Tok->getLen());
  auto Iter = Macros.find(Name);
  if (Iter == Macros.end())
    return false;

  MacroInfo &MI = Iter->second;
  if (BuiltinMacroFn Handler = MI.getHandler()) {
    Token *Expanded = Handler(*this, Tok);
    Expanded->setNext(Tok->getNext());
    Expanded->setAtStartOfLine(Tok->isAtStartOfLine());
    Expanded->setHasLeadingSpace(Tok->hasLeadingSpace());
    Rest = Expanded;
    return true;
  }

  Token *ExpansionEnd = Tok->getNext();
  if (MI.isFunctionLike() && ExpansionEnd->isNot(Token::TK_LParen))
    return false;

  if (MI.isDisabled()) {
    Tok->disableExpand();
    return false;
  }

  std::vector<std::vector<const Token *>> Arguments;
  if (MI.isFunctionLike()) {
    Token *ArgTok = ExpansionEnd->getNext();
    const auto &Parameters = MI.parameters();
    std::size_t NamedCount =
        MI.isVariadic() ? Parameters.size() - 1 : Parameters.size();

    if (Parameters.empty()) {
      if (ArgTok->isNot(Token::TK_RParen))
        Diag.fatalAt(ArgTok->getLoc(), "expected ')'");
    } else {
      for (std::size_t I = 0; I < NamedCount; ++I) {
        std::vector<const Token *> Argument;
        unsigned ParenDepth = 0;
        while (ArgTok->isNot(Token::TK_EOF)) {
          if (ArgTok->is(Token::TK_LParen)) {
            ++ParenDepth;
          } else if (ArgTok->is(Token::TK_RParen)) {
            if (ParenDepth == 0)
              break;
            --ParenDepth;
          } else if (ArgTok->is(Token::TK_Comma) && ParenDepth == 0) {
            break;
          }

          Argument.push_back(ArgTok);
          ArgTok = ArgTok->getNext();
        }
        Arguments.push_back(std::move(Argument));

        if (I + 1 < NamedCount) {
          if (ArgTok->isNot(Token::TK_Comma))
            Diag.fatalAt(ArgTok->getLoc(), "too few arguments");
          ArgTok = ArgTok->getNext();
        }
      }

      if (MI.isVariadic()) {
        std::vector<const Token *> Argument;
        if (ArgTok->is(Token::TK_RParen)) {
          // Empty __VA_ARGS__.
        } else {
          if (NamedCount != 0) {
            if (ArgTok->isNot(Token::TK_Comma))
              Diag.fatalAt(ArgTok->getLoc(), "too few arguments");
            ArgTok = ArgTok->getNext();
          }

          // Collect the remaining tokens, including commas.
          unsigned ParenDepth = 0;
          while (ArgTok->isNot(Token::TK_EOF)) {
            if (ArgTok->is(Token::TK_LParen)) {
              ++ParenDepth;
            } else if (ArgTok->is(Token::TK_RParen)) {
              if (ParenDepth == 0)
                break;
              --ParenDepth;
            }

            Argument.push_back(ArgTok);
            ArgTok = ArgTok->getNext();
          }
        }
        Arguments.push_back(std::move(Argument));
      } else if (ArgTok->is(Token::TK_Comma)) {
        Diag.fatalAt(ArgTok->getLoc(), "too many arguments");
      }

      if (ArgTok->isNot(Token::TK_RParen))
        Diag.fatalAt(ArgTok->getLoc(), "expected ')'");
    }
    ExpansionEnd = ArgTok->getNext();
  }

  std::vector<std::vector<const Token *>> ExpandedArguments;
  ExpandedArguments.reserve(Arguments.size());
  for (const auto &Argument : Arguments)
    ExpandedArguments.push_back(expandMacroArgument(Argument));

  MI.disableMacro();
  Token Head;
  Token *Curr = &Head;
  const auto &ReplacementTokens = MI.tokens();
  const auto &Parameters = MI.parameters();

  for (std::size_t I = 0; I < ReplacementTokens.size(); ++I) {
    const Token &Replacement = ReplacementTokens[I];

    // [250] __VA_OPT__(x): empty if __VA_ARGS__ is empty, else x.
    if (MI.isVariadic() &&
        std::string_view(Replacement.getLoc(), Replacement.getLen()) ==
            "__VA_OPT__" &&
        I + 1 < ReplacementTokens.size() &&
        ReplacementTokens[I + 1].is(Token::TK_LParen)) {
      unsigned Depth = 1;
      std::size_t J = I + 2;
      for (; J < ReplacementTokens.size(); ++J) {
        if (ReplacementTokens[J].is(Token::TK_LParen))
          ++Depth;
        else if (ReplacementTokens[J].is(Token::TK_RParen)) {
          if (--Depth == 0)
            break;
        }
      }
      if (J >= ReplacementTokens.size())
        Diag.fatalAt(Replacement.getLoc(), "unterminated __VA_OPT__");

      if (!Arguments.back().empty()) {
        bool First = true;
        for (std::size_t K = I + 2; K < J; ++K) {
          void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
          Token *Expanded = new (Mem) Token(ReplacementTokens[K]);
          Expanded->setNext(nullptr);
          Expanded->setSourceRange(*Tok);
          Expanded->setOrigin(Tok);
          if (First) {
            Expanded->setAtStartOfLine(Replacement.isAtStartOfLine());
            Expanded->setHasLeadingSpace(Replacement.hasLeadingSpace());
            First = false;
          }
          Curr->setNext(Expanded);
          Curr = Expanded;
        }
      }
      I = J;
      continue;
    }

    if (MI.isFunctionLike() && Replacement.is(Token::TK_Hash)) {
      if (++I == ReplacementTokens.size())
        Diag.fatalAt(Replacement.getLoc(),
                     "'#' is not followed by a macro parameter");

      std::size_t Index = getParameterIndex(MI, ReplacementTokens[I]);
      if (Index == Parameters.size())
        Diag.fatalAt(ReplacementTokens[I].getLoc(),
                     "'#' is not followed by a macro parameter");

      std::string Value;
      for (std::size_t J = 0; J < Arguments[Index].size(); ++J) {
        const Token *ArgTok = Arguments[Index][J];
        if (J != 0 && ArgTok->hasLeadingSpace())
          Value += ' ';
        Value.append(ArgTok->getLoc(), ArgTok->getLen());
      }

      std::string Spelling = "\"";
      for (char C : Value) {
        if (C == '\\' || C == '"')
          Spelling += '\\';
        Spelling += C;
      }
      Spelling += '"';

      char *Buffer = static_cast<char *>(
          MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
      std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);
      void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
      Token *Expanded = new (Mem)
          Token(Token::TK_StrLiteral, Buffer, Buffer + Spelling.size());
      Expanded->setSourceRange(*Tok);
      Expanded->setOrigin(Tok);
      Expanded->setHasLeadingSpace(Replacement.hasLeadingSpace());
      Curr->setNext(Expanded);
      Curr = Expanded;
      continue;
    }

    if (Replacement.is(Token::TK_HashHash))
      Diag.fatalAt(Replacement.getLoc(),
                   "'##' cannot appear at start of macro expansion");

    std::vector<Token *> Operand;
    bool IsPasteChain = I + 1 < ReplacementTokens.size() &&
                        ReplacementTokens[I + 1].is(Token::TK_HashHash);
    const auto &SubstitutionArguments =
        IsPasteChain ? Arguments : ExpandedArguments;
    copyOperandTokens(MacroTokenAlloc, MI, SubstitutionArguments, Replacement,
                      Tok, Operand);

    while (I + 1 < ReplacementTokens.size() &&
           ReplacementTokens[I + 1].is(Token::TK_HashHash)) {
      const Token &PasteOp = ReplacementTokens[I + 1];
      if (I + 2 == ReplacementTokens.size())
        Diag.fatalAt(PasteOp.getLoc(),
                     "'##' cannot appear at end of macro expansion");

      std::vector<Token *> RHS;
      copyOperandTokens(MacroTokenAlloc, MI, Arguments,
                        ReplacementTokens[I + 2], Tok, RHS);
      if (Operand.empty()) {
        Operand = std::move(RHS);
      } else if (!RHS.empty()) {
        pasteTokens(MacroTokenAlloc, Lex, Diag, *Operand.back(), *RHS[0],
                    PasteOp);
        for (std::size_t J = 1; J < RHS.size(); ++J)
          Operand.push_back(RHS[J]);
      }

      I += 2;
    }

    if (!Operand.empty()) {
      Operand.front()->setAtStartOfLine(Replacement.isAtStartOfLine());
      Operand.front()->setHasLeadingSpace(Replacement.hasLeadingSpace());
    }

    for (Token *Expanded : Operand) {
      Curr->setNext(Expanded);
      Curr = Expanded;
    }
  }

  Curr->setNext(ExpansionEnd);
  MacroExpansionStack.push_back(MacroExpansionFrame{&MI, ExpansionEnd});
  Rest = Head.getNext();
  Rest->setAtStartOfLine(Tok->isAtStartOfLine());
  Rest->setHasLeadingSpace(Tok->hasLeadingSpace());
  return true;
}

void Preprocessor::finishMacroExpansions(Token *Tok) {
  while (!MacroExpansionStack.empty() &&
         MacroExpansionStack.back().End == Tok) {
    MacroExpansionStack.back().MI->enableMacro();
    MacroExpansionStack.pop_back();
  }
}

Token *Preprocessor::expandMacroExpression(Token *&Rest, Token *Toks) {
  Token LineHead;
  Token *LineCurr = &LineHead;
  bool ExpectDefinedOperand = false;
  while (Toks->isNot(Token::TK_EOF) && !Toks->isAtStartOfLine()) {
    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Copy = new (Mem) Token(*Toks);
    Copy->setNext(nullptr);

    if (ExpectDefinedOperand) {
      if (Copy->isNot(Token::TK_LParen)) {
        Copy->disableExpand();
        ExpectDefinedOperand = false;
      }
    } else if (Copy->is(Token::TK_Ident) &&
               std::string_view(Copy->getLoc(), Copy->getLen()) == "defined") {
      ExpectDefinedOperand = true;
    }

    LineCurr->setNext(Copy);
    LineCurr = Copy;
    Toks = Toks->getNext();
  }
  Rest = Toks;

  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *EOFToken =
      new (Mem) Token(Token::TK_EOF, Toks->getLoc(), Toks->getLoc());
  LineCurr->setNext(EOFToken);

  Token ResultHead;
  Token *ResultCurr = &ResultHead;
  Toks = LineHead.getNext();
  while (Toks->isNot(Token::TK_EOF)) {
    finishMacroExpansions(Toks);
    Token *Tok = Toks;
    if (expandMacro(Toks, Tok))
      continue;

    ResultCurr->setNext(Toks);
    ResultCurr = Toks;
    Toks = Toks->getNext();
  }
  finishMacroExpansions(Toks);
  ResultCurr->setNext(Toks);
  return ResultHead.getNext();
}

static Token *getExpansionPoint(Token *Tmpl) {
  while (Tmpl->getOrigin())
    Tmpl = Tmpl->getOrigin();
  return Tmpl;
}

Token *Preprocessor::handleFileMacro(Preprocessor &PP, Token *Tmpl) {
  return PP.expandFileMacro(Tmpl);
}

Token *Preprocessor::handleLineMacro(Preprocessor &PP, Token *Tmpl) {
  return PP.expandLineMacro(Tmpl);
}

Token *Preprocessor::handleCounterMacro(Preprocessor &PP, Token *Tmpl) {
  return PP.expandCounterMacro(Tmpl);
}

Token *Preprocessor::handleTimestampMacro(Preprocessor &PP, Token *Tmpl) {
  return PP.expandTimestampMacro(Tmpl);
}

Token *Preprocessor::handleBaseFileMacro(Preprocessor &PP, Token *Tmpl) {
  return PP.expandBaseFileMacro(Tmpl);
}

Token *Preprocessor::expandFileMacro(Token *Tmpl) {
  Token *Origin = getExpansionPoint(Tmpl);
  SourceManager &SM = Diag.getSourceManager();
  std::string_view Filename = SM.getFilename(SM.createBeginLocation(Origin));

  std::string Spelling = "\"";
  for (char C : Filename) {
    if (C == '\\' || C == '"')
      Spelling += '\\';
    Spelling += C;
  }
  Spelling += '"';

  char *Buffer = static_cast<char *>(
      MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
  std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *Expanded =
      new (Mem) Token(Token::TK_StrLiteral, Buffer, Buffer + Spelling.size());
  Expanded->setSourceRange(*Origin);
  return Expanded;
}

Token *Preprocessor::expandLineMacro(Token *Tmpl) {
  Token *Origin = getExpansionPoint(Tmpl);
  SourceManager &SM = Diag.getSourceManager();
  unsigned Line = SM.getLineNumber(SM.createBeginLocation(Origin));
  std::string Spelling = std::to_string(Line);

  char *Buffer = static_cast<char *>(
      MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
  std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *Expanded =
      new (Mem) Token(Token::TK_Num, Buffer, Buffer + Spelling.size(), Line);
  Expanded->setSourceRange(*Origin);
  return Expanded;
}

// [GNU] __COUNTER__ expands to successive integers starting from 0.
Token *Preprocessor::expandCounterMacro(Token *Tmpl) {
  Token *Origin = getExpansionPoint(Tmpl);
  int Val = Counter++;
  std::string Spelling = std::to_string(Val);

  char *Buffer = static_cast<char *>(
      MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
  std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *Expanded =
      new (Mem) Token(Token::TK_Num, Buffer, Buffer + Spelling.size(), Val);
  Expanded->setSourceRange(*Origin);
  return Expanded;
}

// [GNU] __TIMESTAMP__ is the last modification time of the current file,
// e.g. "Fri Jul 24 01:32:50 2020" (24 characters).
Token *Preprocessor::expandTimestampMacro(Token *Tmpl) {
  Token *Origin = getExpansionPoint(Tmpl);
  SourceManager &SM = Diag.getSourceManager();
  const FileEntry *FE = SM.getFileEntry(SM.createBeginLocation(Origin));

  char TimeBuf[30];
  struct stat St;
  if (!FE || ::stat(FE->getPath().c_str(), &St) != 0) {
    std::memcpy(TimeBuf, "??? ??? ?? ??:??:?? ????", 25);
  } else {
    ::ctime_r(&St.st_mtime, TimeBuf);
    TimeBuf[24] = '\0';
  }

  std::string Spelling = "\"";
  Spelling += TimeBuf;
  Spelling += '"';

  char *Buffer = static_cast<char *>(
      MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
  std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *Expanded =
      new (Mem) Token(Token::TK_StrLiteral, Buffer, Buffer + Spelling.size());
  Expanded->setSourceRange(*Origin);
  return Expanded;
}

// [GNU] __BASE_FILE__ is the main input filename (not the current #include).
Token *Preprocessor::expandBaseFileMacro(Token *Tmpl) {
  Token *Origin = getExpansionPoint(Tmpl);

  std::string Spelling = "\"";
  for (char C : BaseFile) {
    if (C == '\\' || C == '"')
      Spelling += '\\';
    Spelling += C;
  }
  Spelling += '"';

  char *Buffer = static_cast<char *>(
      MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
  std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *Expanded =
      new (Mem) Token(Token::TK_StrLiteral, Buffer, Buffer + Spelling.size());
  Expanded->setSourceRange(*Origin);
  return Expanded;
}

} // namespace rcc
