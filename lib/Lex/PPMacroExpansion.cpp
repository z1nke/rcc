#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "Lex/Lexer.h"
#include "Lex/MacroInfo.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"

#include <cstring>
#include <new>
#include <string>
#include <string_view>
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
                  const Token &Operand, const Token &MacroNameTok,
                  std::vector<Token *> &Result) {
  std::size_t Index = getParameterIndex(MI, Operand);
  if (Index == MI.parameters().size()) {
    void *Mem = Alloc.allocate(sizeof(Token), alignof(Token));
    Result.push_back(new (Mem) Token(Operand));
    Result.back()->setNext(nullptr);
    Result.back()->setSourceRange(MacroNameTok);
    return;
  }

  for (const Token *Tok : Arguments[Index]) {
    void *Mem = Alloc.allocate(sizeof(Token), alignof(Token));
    Result.push_back(new (Mem) Token(*Tok));
    Result.back()->setNext(nullptr);
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

std::vector<const Token *> Preprocessor::expandMacroArgument(
    const std::vector<const Token *> &Argument) {
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

  std::string_view NameView(Tok->getLoc(), Tok->getLen());
  if (NameView == "__LINE__") {
    SourceManager &SM = Diag.getSourceManager();
    unsigned Line = SM.getLineNumber(SM.createBeginLocation(Tok));
    std::string Spelling = std::to_string(Line);
    char *Buffer = static_cast<char *>(
        MacroTokenAlloc.allocate(Spelling.size() + 1, alignof(char)));
    std::memcpy(Buffer, Spelling.c_str(), Spelling.size() + 1);

    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Expanded =
        new (Mem) Token(Token::TK_Num, Buffer, Buffer + Spelling.size(), Line);
    Expanded->setSourceRange(*Tok);
    Expanded->setNext(Tok->getNext());
    Rest = Expanded;
    return true;
  }

  std::string Name(NameView);
  auto Iter = Macros.find(Name);
  if (Iter == Macros.end())
    return false;

  MacroInfo &MI = Iter->second;
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

    if (Parameters.empty()) {
      if (ArgTok->isNot(Token::TK_RParen))
        Diag.fatalAt(ArgTok->getLoc(), "expected ')'");
    } else {
      for (std::size_t I = 0; I < Parameters.size(); ++I) {
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

        if (I + 1 < Parameters.size()) {
          if (ArgTok->isNot(Token::TK_Comma))
            Diag.fatalAt(ArgTok->getLoc(), "too few arguments");
          ArgTok = ArgTok->getNext();
        } else if (ArgTok->is(Token::TK_Comma)) {
          Diag.fatalAt(ArgTok->getLoc(), "too many arguments");
        }
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
      Expanded->setHasLeadingSpace(Replacement.hasLeadingSpace());
      Curr->setNext(Expanded);
      Curr = Expanded;
      continue;
    }

    if (Replacement.is(Token::TK_HashHash))
      Diag.fatalAt(Replacement.getLoc(),
                   "'##' cannot appear at start of macro expansion");

    std::vector<Token *> Operand;
    bool IsPasteChain =
        I + 1 < ReplacementTokens.size() &&
        ReplacementTokens[I + 1].is(Token::TK_HashHash);
    const auto &SubstitutionArguments =
        IsPasteChain ? Arguments : ExpandedArguments;
    copyOperandTokens(MacroTokenAlloc, MI, SubstitutionArguments, Replacement,
                      *Tok, Operand);

    while (I + 1 < ReplacementTokens.size() &&
           ReplacementTokens[I + 1].is(Token::TK_HashHash)) {
      const Token &PasteOp = ReplacementTokens[I + 1];
      if (I + 2 == ReplacementTokens.size())
        Diag.fatalAt(PasteOp.getLoc(),
                     "'##' cannot appear at end of macro expansion");

      std::vector<Token *> RHS;
      copyOperandTokens(MacroTokenAlloc, MI, Arguments,
                        ReplacementTokens[I + 2], *Tok, RHS);
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

} // namespace rcc
