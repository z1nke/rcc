#include "Basic/Diagnostic.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rcc {

namespace {

struct PPValue {
  std::uint64_t Val = 0;
  bool IsUnsigned = false;

  bool isNonZero() const { return Val != 0; }
  std::int64_t getSignedValue() const { return static_cast<std::int64_t>(Val); }
};

struct EvalContext {
  Token *CurTok;
  Diagnostic &Diag;
  const std::unordered_map<std::string, MacroInfo> &Macros;
};

enum Precedence : unsigned {
  PrecNone,
  PrecConditional,
  PrecLogicalOr,
  PrecLogicalAnd,
  PrecBitwiseOr,
  PrecBitwiseXor,
  PrecBitwiseAnd,
  PrecEquality,
  PrecRelational,
  PrecShift,
  PrecAdd,
  PrecMul,
};

static bool atEnd(const EvalContext &Ctx) {
  return Ctx.CurTok->is(Token::TK_EOF) || Ctx.CurTok->isAtStartOfLine();
}

static bool consume(EvalContext &Ctx, Token::TokenKind Kind) {
  if (atEnd(Ctx) || Ctx.CurTok->isNot(Kind))
    return false;
  Ctx.CurTok = Ctx.CurTok->getNext();
  return true;
}

static void expect(EvalContext &Ctx, Token::TokenKind Kind,
                   const char *Message) {
  if (!consume(Ctx, Kind))
    Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(), "{}", Message);
}

static bool hasSpelling(const Token *Tok, std::string_view Spelling) {
  return std::string_view(Tok->getLoc(), Tok->getLen()) == Spelling;
}

static unsigned getPrecedence(Token::TokenKind Kind) {
  switch (Kind) {
  case Token::TK_Question:
    return PrecConditional;
  case Token::TK_PipePipe:
    return PrecLogicalOr;
  case Token::TK_AmpAmp:
    return PrecLogicalAnd;
  case Token::TK_Pipe:
    return PrecBitwiseOr;
  case Token::TK_Caret:
    return PrecBitwiseXor;
  case Token::TK_Amp:
    return PrecBitwiseAnd;
  case Token::TK_EqualEqual:
  case Token::TK_NotEqual:
    return PrecEquality;
  case Token::TK_Less:
  case Token::TK_LessEqual:
  case Token::TK_Greater:
  case Token::TK_GreaterEqual:
    return PrecRelational;
  case Token::TK_LessLess:
  case Token::TK_GreaterGreater:
    return PrecShift;
  case Token::TK_Plus:
  case Token::TK_Minus:
    return PrecAdd;
  case Token::TK_Star:
  case Token::TK_Slash:
  case Token::TK_Percent:
    return PrecMul;
  default:
    return PrecNone;
  }
}

static void evaluateDirectiveSubExpr(PPValue &LHS, unsigned MinPrec,
                                     bool ValueLive, EvalContext &Ctx);

static void evaluateDefined(PPValue &Result, EvalContext &Ctx) {
  Ctx.CurTok = Ctx.CurTok->getNext();
  bool HasParen = consume(Ctx, Token::TK_LParen);
  if (atEnd(Ctx) || Ctx.CurTok->isNot(Token::TK_Ident))
    Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(),
                     "expected identifier after 'defined'");

  std::string Name(Ctx.CurTok->getLoc(), Ctx.CurTok->getLen());
  Result.Val = Name == "__LINE__" || Ctx.Macros.contains(Name);
  Result.IsUnsigned = false;
  Ctx.CurTok = Ctx.CurTok->getNext();
  if (HasParen)
    expect(Ctx, Token::TK_RParen, "expected ')'");
}

static void evaluateValue(PPValue &Result, bool ValueLive, EvalContext &Ctx) {
  if (atEnd(Ctx))
    Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(), "expected an expression");

  if (Ctx.CurTok->is(Token::TK_Ident) && hasSpelling(Ctx.CurTok, "defined")) {
    evaluateDefined(Result, Ctx);
    return;
  }

  if (consume(Ctx, Token::TK_LParen)) {
    evaluateValue(Result, ValueLive, Ctx);
    evaluateDirectiveSubExpr(Result, PrecConditional, ValueLive, Ctx);
    expect(Ctx, Token::TK_RParen, "expected ')'");
    return;
  }

  if (consume(Ctx, Token::TK_Plus)) {
    evaluateValue(Result, ValueLive, Ctx);
    return;
  }

  if (consume(Ctx, Token::TK_Minus)) {
    evaluateValue(Result, ValueLive, Ctx);
    Result.Val = 0 - Result.Val;
    return;
  }

  if (consume(Ctx, Token::TK_Exclaim)) {
    evaluateValue(Result, ValueLive, Ctx);
    Result.Val = !Result.isNonZero();
    Result.IsUnsigned = false;
    return;
  }

  if (consume(Ctx, Token::TK_Tilde)) {
    evaluateValue(Result, ValueLive, Ctx);
    Result.Val = ~Result.Val;
    return;
  }

  if (Ctx.CurTok->is(Token::TK_Num)) {
    if (Ctx.CurTok->isFloatingLiteral())
      Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(),
                       "floating constant in preprocessor expression");

    Result.Val = static_cast<std::uint64_t>(Ctx.CurTok->getVal());
    using NLK = Token::NumericLiteralKind;
    NLK Kind = Ctx.CurTok->getNumericLiteralKind();
    Result.IsUnsigned =
        Kind == NLK::UInt || Kind == NLK::ULong || Kind == NLK::ULongLong;
    Ctx.CurTok = Ctx.CurTok->getNext();
    return;
  }

  if (Ctx.CurTok->is(Token::TK_CharLiteral)) {
    Result.Val = Ctx.CurTok->getCharLiteral(Ctx.Diag);
    Ctx.CurTok = Ctx.CurTok->getNext();
    return;
  }

  if (Ctx.CurTok->is(Token::TK_Ident)) {
    Result = {};
    Ctx.CurTok = Ctx.CurTok->getNext();
    return;
  }

  Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(),
                   "invalid token in preprocessor expression");
}

static std::uint64_t getShiftAmount(const PPValue &RHS, bool ValueLive,
                                    EvalContext &Ctx) {
  if (RHS.Val < 64)
    return RHS.Val;
  if (ValueLive)
    Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(), "invalid shift count");
  return 0;
}

static void applyBinaryOperator(PPValue &LHS, const PPValue &RHS,
                                Token::TokenKind Op, bool ValueLive,
                                EvalContext &Ctx) {
  bool IsUnsigned = LHS.IsUnsigned || RHS.IsUnsigned;
  auto SignedLHS = LHS.getSignedValue();
  auto SignedRHS = RHS.getSignedValue();

  switch (Op) {
  case Token::TK_Percent:
    if (RHS.Val == 0) {
      if (ValueLive)
        Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(), "division by zero");
      LHS.Val = 0;
    } else if (IsUnsigned) {
      LHS.Val %= RHS.Val;
    } else {
      LHS.Val = static_cast<std::uint64_t>(SignedLHS % SignedRHS);
    }
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_Slash:
    if (RHS.Val == 0) {
      if (ValueLive)
        Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(), "division by zero");
      LHS.Val = 0;
    } else if (IsUnsigned) {
      LHS.Val /= RHS.Val;
    } else if (SignedLHS == std::numeric_limits<std::int64_t>::min() &&
               SignedRHS == -1) {
      LHS.Val = static_cast<std::uint64_t>(SignedLHS);
    } else {
      LHS.Val = static_cast<std::uint64_t>(SignedLHS / SignedRHS);
    }
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_Star:
    LHS.Val *= RHS.Val;
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_Plus:
    LHS.Val += RHS.Val;
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_Minus:
    LHS.Val -= RHS.Val;
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_LessLess:
    LHS.Val <<= getShiftAmount(RHS, ValueLive, Ctx);
    return;
  case Token::TK_GreaterGreater: {
    std::uint64_t Amount = getShiftAmount(RHS, ValueLive, Ctx);
    LHS.Val = LHS.IsUnsigned ? LHS.Val >> Amount
                             : static_cast<std::uint64_t>(SignedLHS >> Amount);
    return;
  }
  case Token::TK_Less:
    LHS.Val = IsUnsigned ? LHS.Val < RHS.Val : SignedLHS < SignedRHS;
    break;
  case Token::TK_LessEqual:
    LHS.Val = IsUnsigned ? LHS.Val <= RHS.Val : SignedLHS <= SignedRHS;
    break;
  case Token::TK_Greater:
    LHS.Val = IsUnsigned ? LHS.Val > RHS.Val : SignedLHS > SignedRHS;
    break;
  case Token::TK_GreaterEqual:
    LHS.Val = IsUnsigned ? LHS.Val >= RHS.Val : SignedLHS >= SignedRHS;
    break;
  case Token::TK_EqualEqual:
    LHS.Val = LHS.Val == RHS.Val;
    break;
  case Token::TK_NotEqual:
    LHS.Val = LHS.Val != RHS.Val;
    break;
  case Token::TK_Amp:
    LHS.Val &= RHS.Val;
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_Caret:
    LHS.Val ^= RHS.Val;
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_Pipe:
    LHS.Val |= RHS.Val;
    LHS.IsUnsigned = IsUnsigned;
    return;
  case Token::TK_AmpAmp:
    LHS.Val = LHS.isNonZero() && RHS.isNonZero();
    break;
  case Token::TK_PipePipe:
    LHS.Val = LHS.isNonZero() || RHS.isNonZero();
    break;
  default:
    Ctx.Diag.fatalAt(Ctx.CurTok->getLoc(),
                     "invalid operator in preprocessor expression");
  }

  LHS.IsUnsigned = false;
}

static void evaluateDirectiveSubExpr(PPValue &LHS, unsigned MinPrec,
                                     bool ValueLive, EvalContext &Ctx) {
  unsigned PeekPrec =
      atEnd(Ctx) ? PrecNone : getPrecedence(Ctx.CurTok->getKind());

  while (PeekPrec >= MinPrec) {
    Token::TokenKind Operator = Ctx.CurTok->getKind();
    bool RHSIsLive = ValueLive;
    if (Operator == Token::TK_AmpAmp && !LHS.isNonZero())
      RHSIsLive = false;
    else if (Operator == Token::TK_PipePipe && LHS.isNonZero())
      RHSIsLive = false;
    else if (Operator == Token::TK_Question && !LHS.isNonZero())
      RHSIsLive = false;

    Ctx.CurTok = Ctx.CurTok->getNext();
    PPValue RHS;
    evaluateValue(RHS, RHSIsLive, Ctx);

    unsigned ThisPrec = PeekPrec;
    PeekPrec = atEnd(Ctx) ? PrecNone : getPrecedence(Ctx.CurTok->getKind());
    unsigned RHSPrec =
        Operator == Token::TK_Question ? PrecConditional : ThisPrec + 1;
    if (PeekPrec >= RHSPrec) {
      evaluateDirectiveSubExpr(RHS, RHSPrec, RHSIsLive, Ctx);
      PeekPrec = atEnd(Ctx) ? PrecNone : getPrecedence(Ctx.CurTok->getKind());
    }

    if (Operator == Token::TK_Question) {
      expect(Ctx, Token::TK_Colon, "expected ':'");
      bool AfterColonLive = ValueLive && !LHS.isNonZero();
      PPValue AfterColon;
      evaluateValue(AfterColon, AfterColonLive, Ctx);
      evaluateDirectiveSubExpr(AfterColon, ThisPrec, AfterColonLive, Ctx);

      bool Condition = LHS.isNonZero();
      LHS = Condition ? RHS : AfterColon;
      LHS.IsUnsigned = RHS.IsUnsigned || AfterColon.IsUnsigned;
      PeekPrec = atEnd(Ctx) ? PrecNone : getPrecedence(Ctx.CurTok->getKind());
      continue;
    }

    applyBinaryOperator(LHS, RHS, Operator, ValueLive, Ctx);
  }
}

} // namespace

std::int64_t Preprocessor::evaluateDirectiveExpression(Token *&Rest,
                                                       Token *Toks) {
  Token *ExpandedToks = expandMacroExpression(Rest, Toks);
  EvalContext Ctx{ExpandedToks, Diag, Macros};
  if (atEnd(Ctx))
    Diag.fatalAt(ExpandedToks->getLoc(), "no expression");

  PPValue Result;
  evaluateValue(Result, true, Ctx);
  evaluateDirectiveSubExpr(Result, PrecConditional, true, Ctx);
  if (!atEnd(Ctx))
    Diag.fatalAt(Ctx.CurTok->getLoc(), "extra token");

  return static_cast<std::int64_t>(Result.Val);
}

} // namespace rcc
