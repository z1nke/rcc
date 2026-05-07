#include "Basic/Diagnostic.h"
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

bool Preprocessor::expandMacro(Token *&Rest, Token *Tok) {
  if (!isMacroIdentifier(Tok) || Tok->isExpandDisabled())
    return false;

  std::string_view NameView(Tok->getLoc(), Tok->getLen());
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
      Expanded->setSourceRange(Replacement);
      Expanded->setHasLeadingSpace(Replacement.hasLeadingSpace());
      Curr->setNext(Expanded);
      Curr = Expanded;
      continue;
    }

    std::size_t Index = getParameterIndex(MI, Replacement);
    if (Index != Parameters.size()) {
      for (const Token *ArgTok : Arguments[Index]) {
        void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
        Token *Expanded = new (Mem) Token(*ArgTok);
        Expanded->setNext(nullptr);
        Curr->setNext(Expanded);
        Curr = Expanded;
      }
      continue;
    }

    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Expanded = new (Mem) Token(Replacement);
    Expanded->setNext(nullptr);
    Curr->setNext(Expanded);
    Curr = Expanded;
  }

  Curr->setNext(ExpansionEnd);
  MacroExpansionStack.push_back(MacroExpansionFrame{&MI, ExpansionEnd});
  Rest = Head.getNext();
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
  while (Toks->isNot(Token::TK_EOF) && !Toks->isAtStartOfLine()) {
    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Copy = new (Mem) Token(*Toks);
    Copy->setNext(nullptr);
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
