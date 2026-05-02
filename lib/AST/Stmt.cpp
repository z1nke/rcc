#include "AST/Stmt.h"
#include "AST/ASTContext.h"
#include "AST/ASTDumper.h"
#include "AST/Decl.h"
#include "Support/Casting.h"
#include "Support/Unreachable.h"

namespace rcc {

const char *Stmt::getKindStr() const {
  switch (Kind) {
#define STMT(KIND)                                                             \
  case SK_##KIND:                                                              \
    return #KIND;
#include "AST/Stmt.def"
  default:
    RCC_UNREACHABLE("Unknown statement");
  }
}

DeclStmt::DeclStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                   std::vector<Decl *> Decls)
    : Stmt(SK_DeclStmt, BegLoc, EndLoc), Decls(std::move(Decls)) {}

DeclStmt *DeclStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, std::vector<Decl *> Decls) {
  void *Mem = Ctx.allocate(sizeof(DeclStmt), alignof(DeclStmt));
  return new (Mem) DeclStmt(BegLoc, EndLoc, std::move(Decls));
}

CompoundStmt::CompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                           std::vector<Stmt *> Body)
    : Stmt(SK_CompoundStmt, BegLoc, EndLoc), Body(std::move(Body)) {}

CompoundStmt *CompoundStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                                   SourceLocation EndLoc,
                                   std::vector<Stmt *> Body) {
  void *Mem = Ctx.allocate(sizeof(CompoundStmt), alignof(CompoundStmt));
  return new (Mem) CompoundStmt(BegLoc, EndLoc, std::move(Body));
}

ReturnStmt::ReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                       Expr *RetVal)
    : Stmt(SK_ReturnStmt, BegLoc, EndLoc), RetVal(RetVal) {}

ReturnStmt *ReturnStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation EndLoc, Expr *RetVal) {
  void *Mem = Ctx.allocate(sizeof(ReturnStmt), alignof(ReturnStmt));
  return new (Mem) ReturnStmt(BegLoc, EndLoc, RetVal);
}

NullStmt::NullStmt(SourceLocation BegLoc, SourceLocation EndLoc)
    : Stmt(SK_NullStmt, BegLoc, EndLoc) {}

NullStmt *NullStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc) {
  void *Mem = Ctx.allocate(sizeof(NullStmt), alignof(NullStmt));
  return new (Mem) NullStmt(BegLoc, EndLoc);
}

IfStmt::IfStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
               Stmt *Then, Stmt *Else)
    : Stmt(SK_IfStmt, BegLoc, EndLoc), Cond(Cond), Then(Then), Else(Else) {}

IfStmt *IfStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                       SourceLocation EndLoc, Expr *Cond, Stmt *Then,
                       Stmt *Else) {
  void *Mem = Ctx.allocate(sizeof(IfStmt), alignof(IfStmt));
  return new (Mem) IfStmt(BegLoc, EndLoc, Cond, Then, Else);
}

ForStmt::ForStmt(SourceLocation BegLoc, SourceLocation EndLoc, Stmt *Init,
                 Expr *Cond, Expr *Inc, Stmt *Body)
    : Stmt(SK_ForStmt, BegLoc, EndLoc), Init(Init), Cond(Cond), Inc(Inc),
      Body(Body) {}

ForStmt *ForStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                         SourceLocation EndLoc, Stmt *Init, Expr *Cond,
                         Expr *Inc, Stmt *Body) {
  void *Mem = Ctx.allocate(sizeof(ForStmt), alignof(ForStmt));
  return new (Mem) ForStmt(BegLoc, EndLoc, Init, Cond, Inc, Body);
}

WhileStmt::WhileStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
                     Stmt *Body)
    : Stmt(SK_WhileStmt, BegLoc, EndLoc), Cond(Cond), Body(Body) {}

WhileStmt *WhileStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, Expr *Cond, Stmt *Body) {
  void *Mem = Ctx.allocate(sizeof(WhileStmt), alignof(WhileStmt));
  return new (Mem) WhileStmt(BegLoc, EndLoc, Cond, Body);
}

DoWhileStmt::DoWhileStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                         Stmt *Body, Expr *Cond)
    : Stmt(SK_DoWhileStmt, BegLoc, EndLoc), Body(Body), Cond(Cond) {}

DoWhileStmt *DoWhileStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, Stmt *Body,
                                 Expr *Cond) {
  void *Mem = Ctx.allocate(sizeof(DoWhileStmt), alignof(DoWhileStmt));
  return new (Mem) DoWhileStmt(BegLoc, EndLoc, Body, Cond);
}

SwitchStmt::SwitchStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
                       Stmt *Body, SwitchCaseStmt *FirstCase)
    : Stmt(SK_SwitchStmt, BegLoc, EndLoc), Cond(Cond), Body(Body),
      FirstCase(FirstCase) {}

SwitchStmt *SwitchStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation EndLoc, Expr *Cond, Stmt *Body,
                               SwitchCaseStmt *FirstCase) {
  void *Mem = Ctx.allocate(sizeof(SwitchStmt), alignof(SwitchStmt));
  return new (Mem) SwitchStmt(BegLoc, EndLoc, Cond, Body, FirstCase);
}

CaseStmt::CaseStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *LHS,
                   Stmt *SubStmt, std::int64_t CaseValue, unsigned LabelId)
    : SwitchCaseStmt(SK_CaseStmt, BegLoc, EndLoc, SubStmt, LabelId), LHS(LHS),
      CaseValue(CaseValue) {}

CaseStmt *CaseStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, Expr *LHS, Stmt *SubStmt,
                           std::int64_t CaseValue, unsigned LabelId) {
  void *Mem = Ctx.allocate(sizeof(CaseStmt), alignof(CaseStmt));
  return new (Mem) CaseStmt(BegLoc, EndLoc, LHS, SubStmt, CaseValue, LabelId);
}

DefaultStmt::DefaultStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                         Stmt *SubStmt, unsigned LabelId)
    : SwitchCaseStmt(SK_DefaultStmt, BegLoc, EndLoc, SubStmt, LabelId) {}

DefaultStmt *DefaultStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, Stmt *SubStmt,
                                 unsigned LabelId) {
  void *Mem = Ctx.allocate(sizeof(DefaultStmt), alignof(DefaultStmt));
  return new (Mem) DefaultStmt(BegLoc, EndLoc, SubStmt, LabelId);
}

BreakStmt::BreakStmt(SourceLocation BegLoc, SourceLocation EndLoc)
    : Stmt(SK_BreakStmt, BegLoc, EndLoc) {}

BreakStmt *BreakStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc) {
  void *Mem = Ctx.allocate(sizeof(BreakStmt), alignof(BreakStmt));
  return new (Mem) BreakStmt(BegLoc, EndLoc);
}

ContinueStmt::ContinueStmt(SourceLocation BegLoc, SourceLocation EndLoc)
    : Stmt(SK_ContinueStmt, BegLoc, EndLoc) {}

ContinueStmt *ContinueStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                                   SourceLocation EndLoc) {
  void *Mem = Ctx.allocate(sizeof(ContinueStmt), alignof(ContinueStmt));
  return new (Mem) ContinueStmt(BegLoc, EndLoc);
}

GotoStmt::GotoStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                   LabelDecl *Label)
    : Stmt(SK_GotoStmt, BegLoc, EndLoc), Label(Label) {}

GotoStmt *GotoStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, LabelDecl *Label) {
  void *Mem = Ctx.allocate(sizeof(GotoStmt), alignof(GotoStmt));
  return new (Mem) GotoStmt(BegLoc, EndLoc, Label);
}

LabelStmt::LabelStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                     LabelDecl *Label, Stmt *Sub)
    : Stmt(SK_LabelStmt, BegLoc, EndLoc), Label(Label), SubStmt(Sub) {}

LabelStmt *LabelStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, LabelDecl *Label,
                             Stmt *Sub) {
  void *Mem = Ctx.allocate(sizeof(LabelStmt), alignof(LabelStmt));
  return new (Mem) LabelStmt(BegLoc, EndLoc, Label, Sub);
}

UnaryOperator::UnaryOperator(SourceLocation BegLoc, SourceLocation EndLoc,
                             QualType T, Expr *SubExpr, Opcode Op)
    : Expr(SK_UnaryOperator, BegLoc, EndLoc, T), SubExpr(SubExpr), Kind(Op) {}

UnaryOperator *UnaryOperator::create(ASTContext &Ctx, SourceLocation BegLoc,
                                     SourceLocation EndLoc, QualType T,
                                     Expr *SubExpr, Opcode Op) {
  void *Mem = Ctx.allocate(sizeof(UnaryOperator), alignof(UnaryOperator));
  return new (Mem) UnaryOperator(BegLoc, EndLoc, T, SubExpr, Op);
}

const char *UnaryOperator::getOpcodeStr(Opcode Op) {
  switch (Op) {
  case UO_Plus:
    return "+";
  case UO_Minus:
    return "-";
  case UO_LNot:
    return "!";
  case UO_Not:
    return "~";
  case UO_Addrof:
    return "&";
  case UO_Deref:
    return "*";
  case UO_PreInc:
  case UO_PostInc:
    return "++";
  case UO_PreDec:
  case UO_PostDec:
    return "--";
  default:
    RCC_UNREACHABLE("[AST] Unknown unary opcode");
  }
}

void Stmt::dump() const {
  ASTDumper Dumper;
  Dumper.visit(this);
}

void Expr::setType(QualType T) {
  Ty = T;
  // FIXME: Temporary handling.
  if (auto *Ref = dynCast<DeclRefExpr>(this))
    Ref->getDecl()->setType(T);
}

static Expr *ignoreExprNodesImpl(Expr *E) { return E; }
template <typename FnTy, typename... FnTys>
static Expr *ignoreExprNodesImpl(Expr *E, FnTy &&Fn, FnTys &&...Fns) {
  return ignoreExprNodesImpl(std::forward<FnTy>(Fn)(E),
                             std::forward<FnTys>(Fns)...);
}

template <typename... FnTys>
static Expr *ignoreExprNodes(Expr *E, FnTys &&...Fns) {
  Expr *LastE = nullptr;
  while (E != LastE) {
    LastE = E;
    E = ignoreExprNodesImpl(E, std::forward<FnTys>(Fns)...);
  }
  return E;
}

template <typename... FnTys>
static const Expr *ignoreExprNodes(const Expr *E, FnTys &&...Fns) {
  return ignoreExprNodes(const_cast<Expr *>(E), std::forward<FnTys>(Fns)...);
}

static Expr *ignoreImpCastSingleStep(Expr *E) {
  if (auto *Cast = dynCast<CastExpr>(E)) {
    if (Cast->isImplicit())
      return Cast->getSubExpr();
  }

  return E;
}

static Expr *ignoreCastSingleStep(Expr *E) {
  if (auto *Cast = dynCast<CastExpr>(E))
    return Cast->getSubExpr();

  return E;
}

static Expr *ignoreParenSingleStep(Expr *E) {
  if (auto *PE = dynCast<ParenExpr>(E))
    return PE->getSubExpr();
  return E;
}

Expr *Expr::ignoreImpCasts() {
  return ignoreExprNodes(this, ignoreImpCastSingleStep);
}

Expr *Expr::ignoreCasts() {
  return ignoreExprNodes(this, ignoreCastSingleStep);
}

Expr *Expr::ignoreParens() {
  return ignoreExprNodes(this, ignoreParenSingleStep);
}

Expr *Expr::ignoreParenImpCasts() {
  return ignoreExprNodes(this, ignoreParenSingleStep, ignoreImpCastSingleStep);
}

Expr *Expr::ignoreParenCasts() {
  return ignoreExprNodes(this, ignoreParenSingleStep, ignoreCastSingleStep);
}

static std::optional<Expr::EvalResult>
evaluateUnaryOperator(const UnaryOperator *UO) {
  auto SubVal = UO->getSubExpr()->evaluateAsInt();
  if (!SubVal)
    return std::nullopt;

  switch (UO->getOpcode()) {
  case UnaryOperator::UO_Plus:
    return SubVal;
  case UnaryOperator::UO_Minus:
    return Expr::EvalResult(-(*SubVal));
  case UnaryOperator::UO_LNot:
    return Expr::EvalResult(!(*SubVal));
  case UnaryOperator::UO_Not:
    return Expr::EvalResult(~(*SubVal));
  default:
    return std::nullopt;
  }
}

static std::optional<Expr::EvalResult>
evaluateUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *UE) {
  return Expr::EvalResult(static_cast<std::uint64_t>(UE->getSize()));
}

static std::optional<Expr::EvalResult>
evaluateBinaryOperator(const BinaryOperator *BO) {
  auto LHSVal = BO->getLHS()->evaluateAsInt();
  auto RHSVal = BO->getRHS()->evaluateAsInt();
  if (!LHSVal || !RHSVal)
    return std::nullopt;

  auto AsU64 = [](std::int64_t Val) -> std::uint64_t {
    return static_cast<std::uint64_t>(Val);
  };

  const QualType LHSTy = BO->getLHS()->getType();
  const QualType RHSTy = BO->getRHS()->getType();
  const QualType ResTy = BO->getType();
  bool IsUnsigned =
      ResTy->isUnsignedIntegerType() || LHSTy->isUnsignedIntegerType();
  bool IsLHSUnsignedOrPointer =
      LHSTy->isUnsignedIntegerType() || LHSTy->isPointerType();
  switch (BO->getOpcode()) {
  case BinaryOperator::BO_Add: {
    if (LHSTy->isPointerType() && RHSTy->isIntegerType()) {
      std::int64_t ElemSize = LHSTy->getPointeeType()->getSize();
      if (ElemSize == 0)
        return std::nullopt;
      return Expr::EvalResult(AsU64(*LHSVal) + (*RHSVal) * ElemSize);
    }
    if (LHSTy->isIntegerType() && RHSTy->isPointerType()) {
      std::int64_t ElemSize = RHSTy->getPointeeType()->getSize();
      if (ElemSize == 0)
        return std::nullopt;
      return Expr::EvalResult(AsU64(*RHSVal) + (*LHSVal) * ElemSize);
    }
    if (LHSTy.isIntegerType() && RHSTy.isIntegerType())
      return Expr::EvalResult((*LHSVal) + (*RHSVal));
    return std::nullopt;
  }
  case BinaryOperator::BO_Sub: {
    if (LHSTy->isPointerType() && RHSTy->isIntegerType()) {
      std::int64_t ElemSize = LHSTy->getPointeeType()->getSize();
      if (ElemSize == 0)
        return std::nullopt;
      return Expr::EvalResult(AsU64(*LHSVal) - (*RHSVal) * ElemSize);
    }
    if (LHSTy->isPointerType() && RHSTy->isPointerType()) {
      std::int64_t ElemSize = LHSTy->getPointeeType()->getSize();
      if (ElemSize == 0)
        return std::nullopt;
      return Expr::EvalResult(((*LHSVal) - (*RHSVal)) / ElemSize);
    }
    if (LHSTy.isIntegerType() && RHSTy.isIntegerType())
      return Expr::EvalResult((*LHSVal) - (*RHSVal));
    return std::nullopt;
  }
  case BinaryOperator::BO_Mul:
    return Expr::EvalResult((*LHSVal) * (*RHSVal));
  case BinaryOperator::BO_Div:
    if (IsUnsigned)
      return Expr::EvalResult(AsU64(*LHSVal) / AsU64(*RHSVal));
    return Expr::EvalResult((*LHSVal) / (*RHSVal));
  case BinaryOperator::BO_Rem:
    if (IsUnsigned)
      return Expr::EvalResult(AsU64(*LHSVal) % AsU64(*RHSVal));
    return Expr::EvalResult((*LHSVal) % (*RHSVal));
  case BinaryOperator::BO_And:
    return Expr::EvalResult((*LHSVal) & (*RHSVal));
  case BinaryOperator::BO_Or:
    return Expr::EvalResult((*LHSVal) | (*RHSVal));
  case BinaryOperator::BO_Xor:
    return Expr::EvalResult((*LHSVal) ^ (*RHSVal));
  case BinaryOperator::BO_Shl:
    return Expr::EvalResult((*LHSVal) << (*RHSVal));
  case BinaryOperator::BO_Shr:
    if (IsUnsigned)
      return Expr::EvalResult(AsU64(*LHSVal) >> AsU64(*RHSVal));
    return Expr::EvalResult((*LHSVal) >> (*RHSVal));
  case BinaryOperator::BO_EQ:
    return Expr::EvalResult((*LHSVal) == (*RHSVal));
  case BinaryOperator::BO_NE:
    return Expr::EvalResult((*LHSVal) != (*RHSVal));
  case BinaryOperator::BO_LT:
    if (IsLHSUnsignedOrPointer)
      return Expr::EvalResult(AsU64(*LHSVal) < AsU64(*RHSVal));
    return Expr::EvalResult((*LHSVal) < (*RHSVal));
  case BinaryOperator::BO_GT:
    if (IsLHSUnsignedOrPointer)
      return Expr::EvalResult(AsU64(*LHSVal) > AsU64(*RHSVal));
    return Expr::EvalResult((*LHSVal) > (*RHSVal));
  case BinaryOperator::BO_LE:
    if (IsLHSUnsignedOrPointer)
      return Expr::EvalResult(AsU64(*LHSVal) <= AsU64(*RHSVal));
    return Expr::EvalResult((*LHSVal) <= (*RHSVal));
  case BinaryOperator::BO_GE:
    if (IsLHSUnsignedOrPointer)
      return Expr::EvalResult(AsU64(*LHSVal) >= AsU64(*RHSVal));
    return Expr::EvalResult((*LHSVal) >= (*RHSVal));
  case BinaryOperator::BO_LAnd:
    return Expr::EvalResult((*LHSVal) && (*RHSVal));
  case BinaryOperator::BO_LOr:
    return Expr::EvalResult((*LHSVal) || (*RHSVal));
  case BinaryOperator::BO_Comma:
    return RHSVal;
  default:
    return std::nullopt;
  }
}

static std::optional<Expr::EvalResult> evaluateCastExpr(const CastExpr *Cast) {
  auto SubVal = Cast->getSubExpr()->evaluateAsInt();
  if (!SubVal)
    return std::nullopt;

  switch (Cast->getCastKind()) {
  case CastExpr::CK_NoOp:
    return SubVal;
  case CastExpr::CK_ToVoid:
    return std::nullopt;
  case CastExpr::CK_BitCast:
    return SubVal;
  case CastExpr::CK_IntegralCast: {
    const QualType ToTy = Cast->getType();
    if (!ToTy->isIntegerType())
      return std::nullopt;

    std::uint64_t UVal = static_cast<std::uint64_t>(*SubVal);
    std::size_t Width = ToTy->getSize() * 8;
    if (Width == 0)
      return std::nullopt;
    if (Width < 64)
      UVal &= ((1ULL << Width) - 1);

    if (ToTy->isBooleanType())
      return Expr::EvalResult(static_cast<bool>(UVal));
    if (ToTy->isSignedIntegerType()) {
      std::int64_t SVal = static_cast<std::int64_t>(UVal);
      if (Width < 64) {
        std::uint64_t SignBit = 1ULL << (Width - 1);
        if (UVal & SignBit)
          SVal |= static_cast<std::int64_t>(~((1ULL << Width) - 1));
      }
      return Expr::EvalResult(SVal);
    }
    return Expr::EvalResult(UVal);
  }
  case CastExpr::CK_PointerToIntegral:
    return Expr::EvalResult(static_cast<std::uint64_t>(*SubVal));
  case CastExpr::CK_IntegralToPointer:
    return Expr::EvalResult(static_cast<std::uint64_t>(*SubVal));
  case CastExpr::CK_FuncToPointerDecay:
  case CastExpr::CK_ArrayToPointerDecay:
    return std::nullopt;
  default:
    return std::nullopt;
  }
}

std::optional<Expr::EvalResult> Expr::evaluate() const {
  QualType T = getType();
  if (T.isNull() || (!T->isArithmeticType() && !T->isPointerType()))
    return std::nullopt;

  switch (getKind()) {
  case Stmt::SK_UnaryOperator:
    return evaluateUnaryOperator(cast<UnaryOperator>(this));
  case Stmt::SK_UnaryExprOrTypeTraitExpr:
    return evaluateUnaryExprOrTypeTraitExpr(
        cast<UnaryExprOrTypeTraitExpr>(this));
  case Stmt::SK_BinaryOperator:
    return evaluateBinaryOperator(cast<BinaryOperator>(this));
  case Stmt::SK_ConditionalOperator: {
    const auto *CO = cast<ConditionalOperator>(this);
    auto CondVal = CO->getCond()->evaluateAsBool();
    if (!CondVal)
      return std::nullopt;
    if (*CondVal)
      return CO->getTrueExpr()->evaluate();
    return CO->getFalseExpr()->evaluate();
  }
  case Stmt::SK_IntegerLiteral: {
    const auto *IL = cast<IntegerLiteral>(this);
    if (T->isSignedIntegerType())
      return IL->getVal();
    return static_cast<std::uint64_t>(IL->getVal());
  }
  case Stmt::SK_CharacterLiteral: {
    const auto *CL = cast<CharacterLiteral>(this);
    return static_cast<std::uint64_t>(CL->getValue());
  }
  case Stmt::SK_StringLiteral:
    // TODO
    return std::nullopt;
  case Stmt::SK_ParenExpr:
    return cast<ParenExpr>(this)->getSubExpr()->evaluate();
  case Stmt::SK_CastExpr:
    return evaluateCastExpr(cast<CastExpr>(this));
  default:
    return std::nullopt;
  }
}

std::optional<std::int64_t> Expr::evaluateAsInt() const {
  auto Val = evaluate();
  if (!Val)
    return std::nullopt;

  return std::visit(
      [](auto &&V) -> std::optional<std::int64_t> {
        using T = std::decay_t<decltype(V)>;
        if constexpr (std::is_integral_v<T>)
          return V;
        return std::nullopt;
      },
      *Val);
}

std::optional<bool> Expr::evaluateAsBool() const {
  QualType QT = getType();
  if (QT.isNull() ||
      (!QT->isBooleanType() && !QT->isIntegerType() && !QT->isPointerType()))
    return std::nullopt;

  auto IntVal = evaluateAsInt();
  if (!IntVal)
    return std::nullopt;
  return *IntVal != 0;
}

BinaryOperator::BinaryOperator(SourceLocation BegLoc, SourceLocation EndLoc,
                               QualType T, SourceLocation OpLoc, Expr *LHS,
                               Expr *RHS, Opcode Op)
    : Expr(SK_BinaryOperator, BegLoc, EndLoc, T), LHS(std::move(LHS)),
      RHS(std::move(RHS)), Kind(Op) {}

BinaryOperator *BinaryOperator::create(ASTContext &Ctx, SourceLocation BegLoc,
                                       SourceLocation EndLoc, QualType T,
                                       SourceLocation OpLoc, Expr *LHS,
                                       Expr *RHS, Opcode Op) {
  void *Mem = Ctx.allocate(sizeof(BinaryOperator), alignof(BinaryOperator));
  return new (Mem) BinaryOperator(BegLoc, EndLoc, T, OpLoc, LHS, RHS, Op);
}

const char *BinaryOperator::getOpcodeStr() const {
  switch (Kind) {
  case BO_Add:
    return "+";
  case BO_Sub:
    return "-";
  case BO_Mul:
    return "*";
  case BO_Div:
    return "/";
  case BO_Rem:
    return "%";
  case BO_Assign:
    return "=";
  case BO_AddAssign:
    return "+=";
  case BO_SubAssign:
    return "-=";
  case BO_MulAssign:
    return "*=";
  case BO_DivAssign:
    return "/=";
  case BO_RemAssign:
    return "%=";
  case BO_AndAssign:
    return "&=";
  case BO_OrAssign:
    return "|=";
  case BO_XorAssign:
    return "^=";
  case BO_ShlAssign:
    return "<<=";
  case BO_ShrAssign:
    return ">>=";
  case BO_And:
    return "&";
  case BO_Or:
    return "|";
  case BO_Xor:
    return "^";
  case BO_Shl:
    return "<<";
  case BO_Shr:
    return ">>";
  case BO_LAnd:
    return "&&";
  case BO_LOr:
    return "||";
  case BO_EQ:
    return "==";
  case BO_NE:
    return "!=";
  case BO_LT:
    return "<";
  case BO_GT:
    return ">";
  case BO_LE:
    return "<=";
  case BO_GE:
    return ">=";
  case BO_Comma:
    return ",";
  default:
    RCC_UNREACHABLE("[AST] Unknown binary opcode");
  }
}

BinaryOperator::Opcode BinaryOperator::getOpForCompoundAssign() const {
  assert(isCompoundAssign());
  switch (getOpcode()) {
  case BO_AddAssign:
    return BO_Add;
  case BO_SubAssign:
    return BO_Sub;
  case BO_MulAssign:
    return BO_Mul;
  case BO_DivAssign:
    return BO_Div;
  case BO_RemAssign:
    return BO_Rem;
  case BO_AndAssign:
    return BO_And;
  case BO_OrAssign:
    return BO_Or;
  case BO_XorAssign:
    return BO_Xor;
  case BO_ShlAssign:
    return BO_Shl;
  case BO_ShrAssign:
    return BO_Shr;
  default:
    RCC_UNREACHABLE("[AST] Unknown compound assignment opcode");
  }
}

ConditionalOperator::ConditionalOperator(SourceLocation BegLoc,
                                         SourceLocation EndLoc, QualType T,
                                         Expr *Cond, Expr *TrueExpr,
                                         Expr *FalseExpr)
    : Expr(SK_ConditionalOperator, BegLoc, EndLoc, T), Cond(Cond),
      TrueExpr(TrueExpr), FalseExpr(FalseExpr) {}

ConditionalOperator *
ConditionalOperator::create(ASTContext &Ctx, SourceLocation BegLoc,
                            SourceLocation EndLoc, QualType T, Expr *Cond,
                            Expr *TrueExpr, Expr *FalseExpr) {
  void *Mem =
      Ctx.allocate(sizeof(ConditionalOperator), alignof(ConditionalOperator));
  return new (Mem)
      ConditionalOperator(BegLoc, EndLoc, T, Cond, TrueExpr, FalseExpr);
}

IntegerLiteral::IntegerLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                               QualType T, std::int64_t Val)
    : Expr(SK_IntegerLiteral, BegLoc, EndLoc, T), Val(Val) {}

IntegerLiteral *IntegerLiteral::create(ASTContext &Ctx, SourceLocation BegLoc,
                                       SourceLocation EndLoc, QualType T,
                                       std::int64_t Val) {
  void *Mem = Ctx.allocate(sizeof(IntegerLiteral), alignof(IntegerLiteral));
  return new (Mem) IntegerLiteral(BegLoc, EndLoc, T, Val);
}

CharacterLiteral *CharacterLiteral::create(ASTContext &Ctx,
                                           SourceLocation BegLoc,
                                           SourceLocation EndLoc, QualType T,
                                           unsigned Val) {
  void *Mem = Ctx.allocate(sizeof(CharacterLiteral), alignof(CharacterLiteral));
  return new (Mem) CharacterLiteral(BegLoc, EndLoc, T, Val);
}

CharacterLiteral::CharacterLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                                   QualType T, unsigned Val)
    : Expr(SK_CharacterLiteral, BegLoc, EndLoc, T), Val(Val) {}

StringLiteral *StringLiteral::create(ASTContext &Ctx, SourceLocation BegLoc,
                                     SourceLocation EndLoc, QualType T,
                                     std::string Str) {
  void *Mem = Ctx.allocate(sizeof(StringLiteral), alignof(StringLiteral));
  return new (Mem) StringLiteral(BegLoc, EndLoc, T, std::move(Str));
}

StringLiteral::StringLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                             QualType T, std::string Str)
    : Expr(SK_StringLiteral, BegLoc, EndLoc, T), Str(std::move(Str)) {}

ParenExpr::ParenExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                     Expr *SubExpr)
    : Expr(SK_ParenExpr, BegLoc, EndLoc, T), SubExpr(SubExpr) {}

ParenExpr *ParenExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, QualType T, Expr *SubExpr) {
  void *Mem = Ctx.allocate(sizeof(ParenExpr), alignof(ParenExpr));
  return new (Mem) ParenExpr(BegLoc, EndLoc, T, SubExpr);
}

DeclRefExpr::DeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                         QualType T, ValueDecl *D)
    : Expr(SK_DeclRefExpr, BegLoc, EndLoc, T), D(D) {}

DeclRefExpr *DeclRefExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T,
                                 ValueDecl *D) {
  void *Mem = Ctx.allocate(sizeof(DeclRefExpr), alignof(DeclRefExpr));
  return new (Mem) DeclRefExpr(BegLoc, EndLoc, T, D);
}

ArraySubscriptExpr::ArraySubscriptExpr(SourceLocation BegLoc,
                                       SourceLocation EndLoc, QualType T,
                                       Expr *LHS, Expr *RHS)
    : Expr(SK_ArraySubscriptExpr, BegLoc, EndLoc, T), LHS(LHS), RHS(RHS) {}

ArraySubscriptExpr *ArraySubscriptExpr::create(ASTContext &Ctx,
                                               SourceLocation BegLoc,
                                               SourceLocation EndLoc,
                                               QualType T, Expr *LHS,
                                               Expr *RHS) {
  static constexpr std::size_t Size = sizeof(ArraySubscriptExpr);
  void *Mem = Ctx.allocate(Size, alignof(ArraySubscriptExpr));
  return new (Mem) ArraySubscriptExpr(BegLoc, EndLoc, T, LHS, RHS);
}

CallExpr::CallExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                   DeclRefExpr *Callee, std::vector<Expr *> Args)
    : Expr(SK_CallExpr, BegLoc, EndLoc, T), Callee(Callee), Args(Args) {}

CallExpr *CallExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, QualType T,
                           DeclRefExpr *Callee, std::vector<Expr *> Args) {
  void *Mem = Ctx.allocate(sizeof(CallExpr), alignof(CallExpr));
  return new (Mem) CallExpr(BegLoc, EndLoc, T, Callee, Args);
}

FunctionDecl *CallExpr::getCalleeDecl() const {
  if (!Callee)
    return nullptr;
  return dynCast<FunctionDecl>(Callee->getDecl());
}

UnaryExprOrTypeTraitExpr *
UnaryExprOrTypeTraitExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T, Expr *Ex) {
  static constexpr std::size_t Size = sizeof(UnaryExprOrTypeTraitExpr);
  void *Mem = Ctx.allocate(Size, alignof(UnaryExprOrTypeTraitExpr));
  return new (Mem) UnaryExprOrTypeTraitExpr(BegLoc, EndLoc, T, Ex);
}

UnaryExprOrTypeTraitExpr *
UnaryExprOrTypeTraitExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T,
                                 const Type *Ty) {
  static constexpr std::size_t Size = sizeof(UnaryExprOrTypeTraitExpr);
  void *Mem = Ctx.allocate(Size, alignof(UnaryExprOrTypeTraitExpr));
  return new (Mem) UnaryExprOrTypeTraitExpr(BegLoc, EndLoc, T, Ty);
}

UnaryExprOrTypeTraitExpr::UnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                                   SourceLocation EndLoc,
                                                   QualType T, Expr *Ex)
    : Expr(SK_UnaryExprOrTypeTraitExpr, BegLoc, EndLoc, T), IsType(false) {
  Argument.Ex = Ex;
}

UnaryExprOrTypeTraitExpr::UnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                                   SourceLocation EndLoc,
                                                   QualType T, const Type *Ty)
    : Expr(SK_UnaryExprOrTypeTraitExpr, BegLoc, EndLoc, T), IsType(true) {
  Argument.Ty = Ty;
}

std::size_t UnaryExprOrTypeTraitExpr::getSize() const {
  if (isArgumentType())
    return Argument.Ty->getSize();

  QualType Ty = Argument.Ex->getType();
  return Ty->getSize();
}

MemberExpr *MemberExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation OpLoc, SourceLocation EndLoc,
                               QualType T, Expr *Base, FieldDecl *Member,
                               bool IsArrow) {
  void *Mem = Ctx.allocate(sizeof(MemberExpr), alignof(MemberExpr));
  return new (Mem) MemberExpr(BegLoc, OpLoc, EndLoc, T, Base, Member, IsArrow);
}

MemberExpr::MemberExpr(SourceLocation BegLoc, SourceLocation OpLoc,
                       SourceLocation EndLoc, QualType T, Expr *Base,
                       FieldDecl *Member, bool IsArrow)
    : Expr(SK_MemberExpr, BegLoc, EndLoc, T), OpLoc(OpLoc), Base(Base),
      Member(Member), IsArrow(IsArrow) {}

CastExpr *CastExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, QualType T, Expr *SubExpr,
                           CastKind CK, bool IsImplicit) {
  void *Mem = Ctx.allocate(sizeof(CastExpr), alignof(CastExpr));
  return new (Mem) CastExpr(BegLoc, EndLoc, T, SubExpr, CK, IsImplicit);
}

CastExpr::CastExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                   Expr *SubExpr, CastKind CK, bool IsImplicit)
    : Expr(SK_CastExpr, BegLoc, EndLoc, T), SubExpr(SubExpr), CK(CK),
      IsImplicit(IsImplicit) {}

const char *CastExpr::getCastKindStr() const {
#define CASE_CASTKIND(K)                                                       \
  case CK_##K:                                                                 \
    return #K;

  switch (CK) {
    CASE_CASTKIND(NoOp);
    CASE_CASTKIND(ToVoid);
    CASE_CASTKIND(BitCast);
    CASE_CASTKIND(IntegralCast);
    CASE_CASTKIND(PointerToIntegral);
    CASE_CASTKIND(IntegralToPointer);
    CASE_CASTKIND(FuncToPointerDecay);
    CASE_CASTKIND(ArrayToPointerDecay);
  default:
    RCC_UNREACHABLE("Unknown cast kind");
  }
#undef CASE_CASTKIND
}

InitListExpr *InitListExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                   SourceLocation EndLoc, QualType T,
                                   std::vector<Expr *> Inits) {
  void *Mem = Ctx.allocate(sizeof(InitListExpr), alignof(InitListExpr));
  return new (Mem) InitListExpr(BegLoc, EndLoc, T, std::move(Inits));
}

InitListExpr::InitListExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::vector<Expr *> Inits)
    : Expr(SK_InitListExpr, BegLoc, EndLoc, T), Inits(std::move(Inits)) {}

CompoundLiteralExpr *CompoundLiteralExpr::create(ASTContext &Ctx,
                                                 SourceLocation BegLoc,
                                                 SourceLocation EndLoc,
                                                 QualType T, VarDecl *Var) {
  void *Mem =
      Ctx.allocate(sizeof(CompoundLiteralExpr), alignof(CompoundLiteralExpr));
  return new (Mem) CompoundLiteralExpr(BegLoc, EndLoc, T, Var);
}

CompoundLiteralExpr::CompoundLiteralExpr(SourceLocation BegLoc,
                                         SourceLocation EndLoc, QualType T,
                                         VarDecl *Var)
    : Expr(SK_CompoundLiteralExpr, BegLoc, EndLoc, T), Var(Var) {}

StmtExpr *StmtExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, QualType T,
                           CompoundStmt *SubStmt) {
  void *Mem = Ctx.allocate(sizeof(StmtExpr), alignof(StmtExpr));
  return new (Mem) StmtExpr(BegLoc, EndLoc, T, SubStmt);
}

StmtExpr::StmtExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                   CompoundStmt *SubStmt)
    : Expr(SK_StmtExpr, BegLoc, EndLoc, T), SubStmt(SubStmt) {}

} // namespace rcc