#include "Sema/Sema.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Type.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceLocation.h"
#include "Sema/DeclSpec.h"
#include "Support/Unreachable.h"

#include <algorithm>
#include <ranges>

namespace rcc {

Decl *Sema::actOnDeclarator(Declarator &D) {
  QualType T = getTypeForDeclarator(D);

  if (D.getDeclSpec().getStorageClassSpec() == DeclSpec::SCS_Typedef)
    return actOnTypedefDecl(D, T);

  if (const auto *FT = dynCast<FunctionType>(T))
    return actOnFunctionDecl(D, FT, nullptr);

  if (CurrScope->isStructScope()) {
    auto *DeclCtx = CurrScope->getDeclContext();
    assert(DeclCtx && isa<RecordDecl>(DeclCtx));
    return actOnFieldDecl(D, T, cast<RecordDecl>(DeclCtx));
  }

  return actOnVarDecl(D, T);
}

VarDecl *Sema::actOnVarDecl(Declarator &D, QualType T) {
  VarDecl *Var = VarDecl::create(Ctx, D.getLocation(), D.getTypeSpecLoc(),
                                 D.getEndLoc(), T, D.getIdent());
  LocalVars.push_back(Var);
  addDecl(Var);
  return Var;
}

TypedefDecl *Sema::actOnTypedefDecl(Declarator &D, QualType T) {
  auto *Typedef = TypedefDecl::create(Ctx, D.getLocation(), D.getTypeSpecLoc(),
                                      D.getEndLoc(), D.getIdent(), T);
  addDecl(Typedef);
  QualType TT = Ctx.getTypedefType(Typedef, T);
  Typedef->setTypeForDecl(TT.getTypePtr());
  return Typedef;
}

FieldDecl *Sema::actOnFieldDecl(Declarator &D, QualType T, RecordDecl *Parent) {
  FieldDecl *Field = FieldDecl::create(Ctx, D.getLocation(), D.getTypeSpecLoc(),
                                       D.getEndLoc(), T, D.getIdent(), Parent);
  addDecl(Field);
  return Field;
}

ParamVarDecl *Sema::actOnParamVarDecl(Declarator &D, unsigned Index) {
  assert(CurrScope->getFlags() & Scope::FnScope);
  QualType T = getTypeForDeclarator(D);
  const DeclSpec &DS = D.getDeclSpec();
  ParamVarDecl *Param =
      ParamVarDecl::create(Ctx, D.getLocation(), DS.getTypeSpecLoc(),
                           D.getEndLoc(), T, D.getIdent(), Index);
  Params.push_back(Param);
  addDecl(Param);
  return Param;
}

void Sema::addDecl(Decl *D) {
  if (auto *ND = dynCast<NamedDecl>(D)) {
    const std::string &Name = ND->getName();
    for (auto *Prev : CurrScope->decls()) {
      if (const auto *PrevND = dynCast<NamedDecl>(Prev)) {
        if (PrevND->getName() == Name) {
          if (PrevND->getKind() != ND->getKind()) {
            Diag.fatalAt(ND->getLocation(),
                         "redefinition of '{}' as different kind of symbol",
                         Name);
          } else {
            Diag.fatalAt(ND->getLocation(), "redefinition of '{}", Name);
          }
        }
      }
    }
  }

  CurrScope->addDecl(D);
}

FunctionDecl *Sema::actOnFunctionDecl(Declarator &D, const FunctionType *FT,
                                      Stmt *Body) {
  return actOnFunctionDecl(Ctx, D.getLocation(), D.getTypeSpecLoc(),
                           D.getEndLoc(), D.getIdent(), FT->getReturnType(),
                           Body);
}

FunctionDecl *Sema::actOnFunctionDecl(ASTContext &Ctx, SourceLocation Loc,
                                      SourceLocation BegLoc,
                                      SourceLocation EndLoc, std::string Name,
                                      QualType RetType, Stmt *Body) {
  auto *Func = FunctionDecl::create(Ctx, Loc, BegLoc, EndLoc,
                                    Ctx.getFunctionType(RetType, {}),
                                    std::move(Name), Body);
  assert(CurrScope->isFunctionScope());
  addDecl(Func);
  Funcs.push_back(Func);
  return Func;
}

RecordDecl *Sema::actOnRecordDecl(SourceLocation Loc, SourceLocation BegLoc,
                                  SourceLocation EndLoc, std::string_view Ident,
                                  unsigned TagKind) {
  return RecordDecl::create(Ctx, Loc, BegLoc, EndLoc, std::string(Ident),
                            static_cast<RecordDecl::TagKind>(TagKind));
}

void Sema::complete(FunctionDecl *FD) {
  std::vector<QualType> ParamTypes;
  for (const auto *Param : Params)
    ParamTypes.push_back(Param->getType());

  std::vector<ParamVarDecl *> PVars;
  std::swap(PVars, Params);
  FD->setParams(std::move(PVars));
  QualType FT = FD->getType();
  const auto *FuncTy = dynCast<FunctionType>(FT);
  if (!FuncTy)
    Diag.fatalAt(FD->getLocation(), "expect function type");

  QualType RetType = FuncTy->getReturnType();
  QualType NewFT = Ctx.getFunctionType(RetType, std::move(ParamTypes));
  FD->setType(NewFT);

  if (!FD->getBody())
    return;

  std::vector<VarDecl *> Vars;
  std::swap(Vars, LocalVars);
  std::ranges::reverse(Vars);
  FD->setLocalVars(std::move(Vars));
}

Expr *Sema::actOnStringLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                               QualType T, std::string Str) {
  return StringLiteral::create(Ctx, BegLoc, EndLoc, T, Str);
}

Stmt *Sema::actOnDeclStmt(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, std::vector<Decl *> Decls) {
  return DeclStmt::create(Ctx, BegLoc, EndLoc, std::move(Decls));
}

Stmt *Sema::actOnNullStmt(SourceLocation SemiLoc) {
  return NullStmt::create(Ctx, SemiLoc, SemiLoc);
}

Stmt *Sema::actOnReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                            Expr *RetVal) {
  QualType RetType;
  if (CurrScopeDecl) {
    const auto *Func = dynCastOrNull<FunctionDecl>(CurrScopeDecl);
    if (Func) {
      QualType FuncType = Func->getType();
      const auto *FT = FuncType->getAs<FunctionType>();
      if (FT)
        RetType = FT->getReturnType();
    }
  }

  Scope *S = CurrScope;
  while (RetType.isNull() && S) {
    Decl *D = S->getDeclContext();
    const auto *Func = dynCastOrNull<FunctionDecl>(D);
    if (!Func) {
      S = S->getParent();
      continue;
    }

    QualType FuncType = Func->getType();
    const auto *FT = FuncType->getAs<FunctionType>();
    if (!FT)
      Diag.fatalAt(Func->getLocation(), "unknown return type");

    RetType = FT->getReturnType();
  }

  if (RetType.isNull())
    Diag.fatalAt(BegLoc, "return statement is not within a function");

  if (RetType.isVoidType()) {
    if (!RetVal->getType().isVoidType())
      RetVal = actOnCastExpr(RetVal->getBeginLoc(), RetVal->getEndLoc(),
                             RetType, RetVal, /*IsImplicit=*/true);
    return ReturnStmt::create(Ctx, BegLoc, EndLoc, RetVal);
  }

  RetVal = usualUnaryConv(RetVal);
  assert(RetVal);

  if (!Ctx.hasSameType(RetType, RetVal->getType())) {
    auto CK = getCastKind(RetType, RetVal->getType());
    if (!CK)
      Diag.fatalAt(RetVal->getBeginLoc(), "invalid return value type");

    if (*CK != CastExpr::CK_NoOp)
      RetVal = impCastExprToType(RetVal, RetType, *CK);
  }

  return ReturnStmt::create(Ctx, BegLoc, EndLoc, RetVal);
}

Stmt *Sema::actOnCompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                              std::vector<Stmt *> Body) {
  return CompoundStmt::create(Ctx, BegLoc, EndLoc, std::move(Body));
}

Stmt *Sema::actOnIfStmt(SourceLocation BegLoc, Expr *Cond, Stmt *Then,
                        Stmt *Else) {
  checkScalarType(Cond->getType());
  auto EndLoc = Else ? Else->getEndLoc() : Then->getEndLoc();
  return IfStmt::create(Ctx, BegLoc, EndLoc, Cond, Then, Else);
}

Stmt *Sema::actOnForStmt(SourceLocation BegLoc, Stmt *Init, Expr *Cond,
                         Expr *Inc, Stmt *Body) {
  if (Cond)
    checkScalarType(Cond->getType());
  auto EndLoc = Body->getEndLoc();
  return ForStmt::create(Ctx, BegLoc, EndLoc, Init, Cond, Inc, Body);
}

Stmt *Sema::actOnWhileStmt(ASTContext &Ctx, SourceLocation BegLoc, Expr *Cond,
                           Stmt *Body) {
  checkScalarType(Cond->getType());
  auto EndLoc = Body->getEndLoc();
  return WhileStmt::create(Ctx, BegLoc, EndLoc, Cond, Body);
}

Expr *Sema::actOnBinaryOperator(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                                unsigned Op) {
  QualType ResType = getBinaryOperatorType(OpLoc, LHS, RHS, Op);
  auto BegLoc = LHS->getBeginLoc();
  auto EndLoc = RHS->getEndLoc();
  return BinaryOperator::create(Ctx, BegLoc, EndLoc, ResType, OpLoc, LHS, RHS,
                                static_cast<BinaryOperator::Opcode>(Op));
}

Expr *Sema::actOnUnaryOperator(SourceLocation OpLoc, Expr *SubExpr,
                               unsigned Op) {
  QualType ResType = getUnaryOperatorType(OpLoc, SubExpr, Op);
  return UnaryOperator::create(Ctx, OpLoc, SubExpr->getEndLoc(), ResType,
                               SubExpr, static_cast<UnaryOperator::Opcode>(Op));
}

Expr *Sema::actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc, Expr *Ex) {
  // FIXME: Fix sizeof type, int -> size_t.
  return UnaryExprOrTypeTraitExpr::create(Ctx, BegLoc, Ex->getEndLoc(),
                                          Ctx.IntTy, Ex);
}

Expr *Sema::actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                          SourceLocation EndLoc,
                                          const Type *Ty) {
  // FIXME: Fix sizeof type, int -> size_t.
  return UnaryExprOrTypeTraitExpr::create(Ctx, BegLoc, EndLoc, Ctx.IntTy, Ty);
}

Expr *Sema::actOnParenExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                           Expr *SubExpr) {
  return ParenExpr::create(Ctx, BegLoc, EndLoc, SubExpr->getType(), SubExpr);
}

Expr *Sema::actOnDeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                             std::string_view Ident) {
  VarDecl *Var = findVar(Ident);
  if (!Var)
    Diag.fatalAt(BegLoc, "undeclared variable '{}'", Ident.data());

  return DeclRefExpr::create(Ctx, BegLoc, EndLoc, Var->getType(), Var);
}

Expr *Sema::actOnCallExpr(SourceLocation IdentBegLoc,
                          SourceLocation IdentEndLoc, SourceLocation EndLoc,
                          std::string_view Name, std::vector<Expr *> Args) {
  FunctionDecl *FD = findFunction(Name);
  if (!FD) {
    // [69] Report an error on undefined/undeclared functions
    Diag.fatalAt(IdentBegLoc, "implicit declaration of a function");

    // Implicit function declaration.
    // FD = FunctionDecl::create(
    //     Ctx, SourceLocation(), SourceLocation(), SourceLocation(),
    //     Ctx.getFunctionType(Ctx.IntTy, {}), std::string(Name), nullptr);
    // Funcs.push_back(FD);
    // FD->setImplicit(true);
  }

  QualType FuncType = FD->getType();
  auto *FT = FuncType->getAs<FunctionType>();
  assert(FT);

  QualType RetType = FT->getReturnType();
  auto *Ref = DeclRefExpr::create(Ctx, IdentBegLoc, IdentEndLoc, RetType, FD);
  return CallExpr::create(Ctx, IdentBegLoc, EndLoc, RetType, Ref,
                          std::move(Args));
}

Expr *Sema::actOnCastExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                          QualType T, Expr *SubExpr, bool IsImplicit) {
  QualType SubT = SubExpr->getType();
  auto CK = CastExpr::CastKind::CK_NoOp;
  do {
    if (T == SubT || T.getTypePtr() == SubT.getTypePtr())
      break;

    if (T.isVoidType()) {
      CK = CastExpr::CK_ToVoid;
      break;
    }

    if (T.isIntegerType()) {
      if (SubT.isIntegerType()) {
        CK = CastExpr::CK_IntegralCast;
        break;
      }

      if (SubT->isPointerType()) {
        CK = CastExpr::CK_PointerToIntegral;
        break;
      }
      break;
    }

    if (T->isPointerType()) {
      if (SubT.isIntegerType()) {
        CK = CastExpr::CK_IntegralToPointer;
        break;
      }

      if (SubT->isPointerType()) {
        if (T->getPointeeType().getTypePtr() ==
            SubT->getPointeeType().getTypePtr())
          break;

        CK = CastExpr::CK_BitCast;
      }
      break;
    }
  } while (false);

  return CastExpr::create(Ctx, BegLoc, EndLoc, T, SubExpr, CK, IsImplicit);
}

Expr *Sema::actOnStmtExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                          Stmt *SubStmt) {
  auto *CS = dynCast<CompoundStmt>(SubStmt);
  if (!CS)
    Diag.fatalAt(BegLoc, "statement expression requires compound statement");

  const auto &Body = CS->getBody();
  if (Body.empty())
    Diag.fatalAt(BegLoc, "statement expression requires non-empty body");

  const auto *Back = dynCast<Expr>(Body.back());
  if (!Back)
    Diag.fatalAt(Body.back()->getBeginLoc(), "expected expression");

  return StmtExpr::create(Ctx, BegLoc, EndLoc, Back->getType(), CS);
}

Expr *Sema::actOnArraySubscriptExpr(SourceLocation EndLoc, Expr *LHS,
                                    Expr *RHS) {
  QualType LType = LHS->getType();
  QualType RType = RHS->getType();
  SourceLocation BegLoc = LHS->getBeginLoc();
  QualType T;
  // base[idx]
  if (QualType ElemType = LType->getPointeeOrArrayElementType()) {
    if (!RType.isIntegerType())
      Diag.fatalAt(RHS->getBeginLoc(),
                   "index-expression requires integer type");
    T = ElemType;
  } else if (QualType ElemType = RType->getPointeeOrArrayElementType()) {
    if (!LType.isIntegerType())
      Diag.fatalAt(LHS->getBeginLoc(),
                   "index-expression requires integer type");
    T = ElemType;
  } else {
    Diag.fatalAt(BegLoc, "base-expression requires pointer or array type");
  }

  return ArraySubscriptExpr::create(Ctx, LHS->getBeginLoc(), EndLoc, T, LHS,
                                    RHS);
}

Expr *Sema::actOnMemberAccessExpr(SourceLocation OpLoc, SourceLocation EndLoc,
                                  Expr *Base, std::string_view Ident,
                                  bool IsArrow) {
  QualType BaseType = Base->getType().getCanonicalType();
  if (IsArrow) {
    QualType PointeeType = BaseType->getPointeeType();
    if (BaseType.isNull()) {
      Diag.fatalAt(Base->getBeginLoc(),
                   "member reference type '{}' is not a pointer",
                   BaseType.getAsString());
    }
    BaseType = PointeeType;
  }

  const auto *Record = BaseType->getAsRecordDecl();
  auto BegLoc = Base->getBeginLoc();
  if (!Record)
    Diag.fatalAt(BegLoc, "member access requires struct or union type");

  // TODO: Record type must be complete.
  for (auto &Field : Record->fields()) {
    if (Field->getName() != Ident)
      continue;
    return MemberExpr::create(Ctx, BegLoc, OpLoc, EndLoc, Field->getType(),
                              Base, Field, IsArrow);
  }

  Diag.fatalAt(BegLoc, "field '{}' not found in record", Ident);
}

void Sema::checkScalarType(QualType T) {
  if (!T->isScalarType())
    Diag.fatalAt(SourceLocation(), "type requires scalar type");
}

void Sema::checkIntType(Expr *E) {
  if (!E->getType().isIntegerType())
    Diag.fatalAt(E->getBeginLoc(), "expression requires integer type");
}

void Sema::checkArithmeticType(Expr *E) {
  if (!E->getType()->isArithmeticType())
    Diag.fatalAt(E->getBeginLoc(), "expression requires arithmetic type");
}

/// usualUnaryConv - Performs various conversions that are common to most
/// operators (C99 6.3). The conversions of array and function types are
/// sometimes suppressed. For example, the array->pointer conversion doesn't
/// apply if the array is an argument to the sizeof or address (&) operators.
/// In these instances, this routine should *not* be called.
Expr *Sema::usualUnaryConv(Expr *E) {
  E = defaultFunctionArrayLvalueConv(E);
  assert(E);

  // TODO: Try to perform integral promotions if the object has a theoretically
  // promotable type.
  // ...

  return E;
}

Expr *Sema::defaultFunctionArrayLvalueConv(Expr *E) {
  return defaultLvalueConv(defaultFunctionArrayConv(E));
}

Expr *Sema::defaultFunctionArrayConv(Expr *E) {
  QualType T = E->getType();
  assert(!T.isNull());

  if (T->isFunctionType()) {
    return impCastExprToType(E, Ctx.getPointerType(T),
                             CastExpr::CK_FuncToPointerDecay);
  }

  if (T->isArraryType()) {
    QualType PtrTy = Ctx.getArrayDecayedType(T);
    assert(!PtrTy.isNull());
    return impCastExprToType(E, PtrTy, CastExpr::CK_ArrayToPointerDecay);
  }

  return E;
}

Expr *Sema::defaultLvalueConv(Expr *E) {
  // TODO: Impl
  return E;
}

using PerformCastFn = Expr *(*)(Sema &, Expr *, QualType);

template <PerformCastFn doLHSCast, PerformCastFn doRHSCast>
static QualType handleArithConv(Sema &S, Expr *&LHS, Expr *&RHS, QualType LType,
                                QualType RType, bool IsCompAssign) {
  const ASTContext &Ctx = S.getASTContext();
  int Order = Ctx.getIntTypeOrder(LType, RType);
  bool IsLS = LType->isSignedIntegerOrEnumerationType();
  bool IsRS = RType->isSignedIntegerOrEnumerationType();
  if (IsLS == IsRS) {
    // Same signedness; use the higher-ranked type
    if (Order >= 0) {
      RHS = (*doRHSCast)(S, RHS, LType);
      return LType;
    }

    if (!IsCompAssign)
      LHS = (*doLHSCast)(S, LHS, RType);
    return RType;
  }

  if (Order != (IsLS ? 1 : -1)) {
    // The unsigned type has greater than or equal rank to the
    // signed type, so use the unsigned type
    if (IsRS) {
      RHS = (*doRHSCast)(S, RHS, LType);
      return LType;
    }
    if (!IsCompAssign)
      LHS = (*doLHSCast)(S, LHS, RType);
    return RType;
  }

  if (Ctx.getIntWidth(LType) != Ctx.getIntWidth(RType)) {
    // The two types are different widths; if we are here, that
    // means the signed type is larger than the unsigned type, so
    // use the signed type.
    if (IsLS) {
      RHS = (*doRHSCast)(S, RHS, LType);
      return LType;
    }
    if (!IsCompAssign)
      LHS = (*doLHSCast)(S, LHS, RType);
    return RType;
  }

  // TODO: Impl
  // The signed type is higher-ranked than the unsigned type,
  // but isn't actually any bigger (like unsigned int and long
  // on most 32-bit systems).  Use the unsigned type corresponding
  // to the signed type.
  return LType;
}

static Expr *doIntegralCast(Sema &S, Expr *E, QualType ToType) {
  return S.impCastExprToType(E, ToType, CastExpr::CK_IntegralCast);
}

/// usualArithConv - Performs various conversions that are common to
/// binary operators (C99 6.3.1.8). If both operands aren't arithmetic, this
/// routine returns the first non-arithmetic type found. The client is
/// responsible for emitting appropriate error diagnostics.
QualType Sema::usualArithConv(Expr *&LHS, Expr *&RHS, ArithConvKind ACK) {
  // TODO: checkEnumArithmeticConversions
  if (ACK != ACK_CompAssign) {
    LHS = usualUnaryConv(LHS);
    assert(LHS);
  }

  RHS = usualUnaryConv(RHS);
  assert(RHS);

  QualType LType = LHS->getType().getUnqualifiedType();
  QualType RType = RHS->getType().getUnqualifiedType();
  if (Ctx.hasSameType(LType, RType))
    return LType;

  if (!LType->isArithmeticType() || !RType->isArithmeticType())
    return QualType();

  return handleArithConv<doIntegralCast, doIntegralCast>(
      *this, LHS, RHS, LType, RType, ACK == ACK_CompAssign);
}

Expr *Sema::impCastExprToType(Expr *E, QualType Ty, unsigned CK) {
  QualType ExprTy = E->getType().getCanonicalType();
  QualType TypeTy = Ty.getCanonicalType();
  if (ExprTy == TypeTy)
    return E;

  return CastExpr::create(Ctx, E->getBeginLoc(), E->getEndLoc(), Ty, E,
                          static_cast<CastExpr::CastKind>(CK),
                          true /*Implicit*/);
}

std::optional<unsigned> Sema::getCastKind(QualType ToType, QualType FromType) {
  if (ToType == FromType)
    return CastExpr::CK_NoOp;

  const auto *ToPtrTy = ToType->getAs<PointerType>();
  const auto *FromPtrTy = FromType->getAs<PointerType>();
  if (ToPtrTy && FromPtrTy) {
    QualType ToPointeeTy = ToPtrTy->getPointeeType();
    QualType FromPointeeTy = FromPtrTy->getPointeeType();
    return ToPointeeTy == FromPointeeTy ? CastExpr::CK_NoOp
                                        : CastExpr::CK_BitCast;
  }

  bool ToIsInt = ToType.isIntegerType();
  bool FromIsInt = FromType.isIntegerType();
  if (ToIsInt && FromIsInt)
    return CastExpr::CK_IntegralCast;

  if (ToPtrTy && FromIsInt)
    return CastExpr::CK_IntegralToPointer;
  if (FromPtrTy && ToIsInt)
    return CastExpr::CK_PointerToIntegral;

  return std::nullopt;
}

QualType Sema::getBinaryOperatorType(SourceLocation OpLoc, Expr *&LHS,
                                     Expr *&RHS, unsigned Op) {
  switch (Op) {
  case BinaryOperator::BO_Assign: {
    QualType LType = LHS->getType();
    if (LType->isArraryType())
      Diag.fatalAt(LHS->getBeginLoc(), "cannot assign to array type");

    RHS = usualUnaryConv(RHS);
    QualType RType = RHS->getType();
    // FIXME: Check LHS type.
    if (LType.isNull()) {
      LType = RType;
      LHS->setType(LType);
      return LType;
    }
    auto CK = getCastKind(LType, RType);
    if (!CK)
      Diag.fatalAt(OpLoc, "invalid assignment operand");

    if (*CK != CastExpr::CK_NoOp)
      RHS = impCastExprToType(RHS, LType, *CK);
    return LType;
  }
  case BinaryOperator::BO_Add:
    return getAddOpType(OpLoc, LHS, RHS);
  case BinaryOperator::BO_Sub:
    return getSubOpType(OpLoc, LHS, RHS);
  case BinaryOperator::BO_Mul:
  case BinaryOperator::BO_Div:
    return getMulDivOpType(OpLoc, LHS, RHS, false);
  case BinaryOperator::BO_EQ:
  case BinaryOperator::BO_NE:
  case BinaryOperator::BO_LT:
  case BinaryOperator::BO_GT:
  case BinaryOperator::BO_LE:
  case BinaryOperator::BO_GE:
    // TODO: Check operands and add implicit expr.
    return Ctx.IntTy;
  case BinaryOperator::BO_Comma:
    return RHS->getType();
  default:
    Diag.fatalAt(OpLoc, "unknown binary opcode");
  }
}

QualType Sema::getAddOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS) {
  LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);
  QualType LType = LHS->getType();
  QualType RType = RHS->getType();
  checkScalarType(LType);
  checkScalarType(RType);
  bool LIsPtr = LType->isPointerType();
  bool RIsPtr = RType->isPointerType();
  if (LIsPtr && RIsPtr)
    Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");

  bool LIsArithmetic = LType->isArithmeticType();
  bool RIsArithmetic = LType->isArithmeticType();
  if (LIsArithmetic && RIsArithmetic)
    return usualArithConv(LHS, RHS, ACK_Arithmetic);

  if (LIsPtr) {
    checkIntType(RHS);
    return LType;
  }

  if (RIsPtr) {
    checkIntType(LHS);
    return RType;
  }

  Diag.fatalAt(OpLoc, "invalid operand");
}

QualType Sema::getSubOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS) {
  LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);
  QualType LType = LHS->getType();
  QualType RType = RHS->getType();
  checkScalarType(LType);
  checkScalarType(RType);
  bool LIsPtr = LType->isPointerType();
  bool RIsPtr = RType->isPointerType();
  if (LIsPtr && RIsPtr)
    return Ctx.IntTy; // FIXME: ptrdiff_t

  bool LIsArithmetic = LType->isArithmeticType();
  bool RIsArithmetic = LType->isArithmeticType();
  if (LIsArithmetic && RIsArithmetic)
    return usualArithConv(LHS, RHS, ACK_Arithmetic);

  if (LIsPtr) {
    checkIntType(RHS);
    return LType;
  }

  Diag.fatalAt(OpLoc, "invalid operand");
}

QualType Sema::getMulDivOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                               bool IsCompAssign) {
  LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);
  if (LHS->getType()->isPointerType())
    Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");
  if (RHS->getType()->isPointerType())
    Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");

  auto ACK = IsCompAssign ? ACK_CompAssign : ACK_Arithmetic;
  return usualArithConv(LHS, RHS, ACK);
}

QualType Sema::getUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
                                    unsigned Op) {
  switch (Op) {
  case UnaryOperator::UO_Plus:
  case UnaryOperator::UO_Minus:
    checkArithmeticType(SubExpr);
    return SubExpr->getType();
  case UnaryOperator::UO_Addrof:
    // FIXME: Temporarily handle array type.
    if (const auto *ArrType = SubExpr->getType()->getAs<ArrayType>())
      return Ctx.getPointerType(ArrType->getElementType());

    return Ctx.getPointerType(SubExpr->getType());
  case UnaryOperator::UO_Deref: {
    QualType Result;
    if (const auto *PtrType = SubExpr->getType()->getAs<PointerType>())
      Result = PtrType->getPointeeType();
    else if (const auto *ArrType = SubExpr->getType()->getAs<ArrayType>())
      Result = ArrType->getElementType();
    else {
      Diag.fatalAt(SubExpr->getBeginLoc(),
                   "dereference requires pointer operand");
    }

    if (Result.isVoidType())
      Diag.fatalAt(SubExpr->getBeginLoc(), "dereferencing a void pointer");
    return Result;
  }
  default:
    Diag.fatalAt(OpLoc, "unknown unary opcode");
  }
}

VarDecl *Sema::findVar(std::string_view Ident) const {
  for (Scope *S = CurrScope; S; S = S->getParent()) {
    for (auto *D : S->decls()) {
      auto *Var = dynCast<VarDecl>(D);
      if (!Var)
        continue;

      if (Var->getName() == Ident)
        return Var;
    }
  }

  return nullptr;
}

TagDecl *Sema::findTagDecl(std::string_view Ident) const {
  for (Scope *S = CurrScope; S; S = S->getParent()) {
    for (auto *Tag : S->tags()) {
      if (Tag->getName() == Ident)
        return Tag;
    }
  }

  return nullptr;
}

FunctionDecl *Sema::findFunction(std::string_view Ident) const {
  for (FunctionDecl *FD : Funcs) {
    if (FD->getName() == Ident)
      return FD;
  }

  return nullptr;
}

TypedefDecl *Sema::findTypedef(std::string_view Ident) const {
  for (Scope *S = CurrScope; S; S = S->getParent()) {
    for (auto *D : S->decls()) {
      auto *TD = dynCast<TypedefDecl>(D);
      if (!TD)
        continue;

      if (TD->getName() == Ident)
        return TD;
    }
  }

  return nullptr;
}

QualType Sema::convertDeclSpecToType(const DeclSpec &DS) {
  QualType T;
  switch (DS.getTypeSpecType()) {
  case DeclSpec::TST_Void:
    T = Ctx.VoidTy;
    break;
  case DeclSpec::TST_Char:
    T = Ctx.CharTy;
    break;
  case DeclSpec::TST_Unspecified:
  case DeclSpec::TST_Int: {
    switch (DS.getTypeSpecWidth()) {
    case DeclSpec::TSW_Unspecified:
      T = Ctx.IntTy;
      break;
    case DeclSpec::TSW_Short:
      T = Ctx.ShortTy;
      break;
    case DeclSpec::TSW_Long:
      T = Ctx.LongTy;
      break;
    case DeclSpec::TSW_LongLong:
      T = Ctx.LongLongTy;
      break;
    default:
      RCC_UNREACHABLE("Unknown type specifier width");
    }
    break;
  }
  case DeclSpec::TST_Struct:
  case DeclSpec::TST_Union: {
    const auto *RD = dynCast<RecordDecl>(DS.getRepDecl());
    if (!RD)
      Diag.fatalAt(DS.getTypeSpecLoc(), "struct/union has no declaration");
    T = RD->getType();
    break;
  }
  case DeclSpec::TST_Typename: {
    const auto *D = DS.getRepDecl();
    assert(D);
    if (const auto *Typedef = dynCast<TypedefDecl>(D)) {
      T = Typedef->getType();
      break;
    }
  }
  default:
    RCC_UNREACHABLE("Unknown type specifier type");
  }

  return T;
}

QualType Sema::getTypeForDeclarator(Declarator &D) {
  const DeclSpec &DS = D.getDeclSpec();
  QualType T = convertDeclSpecToType(DS);
  // Get full type.
  for (const auto &Chunk : (D.getDeclChunks() | std::views::reverse)) {
    switch (Chunk.Kind) {
    case DeclaratorChunk::DCK_Pointer:
      T = Ctx.getPointerType(T);
      break;
    case DeclaratorChunk::DCK_Function:
      T = Ctx.getFunctionType(T, {});
      break;
    case DeclaratorChunk::DCK_Array:
      if (!Chunk.Arr.LenExpr)
        Diag.fatalAt(D.getLocation(), "array size must be constant");
      T = Ctx.getConstantArrayType(T, getArrayLength(Chunk.Arr.LenExpr));
      break;
    default:
      Diag.fatalAt(DS.getTypeSpecLoc(), "unknown declarator type");
    }
  }

  return T;
}

QualType Sema::tryDecayArrayType(QualType T) {
  if (const auto *AT = T->getAs<ArrayType>())
    return Ctx.getPointerType(AT->getElementType());
  return T;
}

std::size_t Sema::getArrayLength(const Expr *E) const {
  if (const auto *IL = dynCast<IntegerLiteral>(E)) {
    std::int64_t Val = IL->getVal();
    if (Val <= 0)
      Diag.fatalAt(IL->getBeginLoc(), "array size must be positive");
    return static_cast<std::size_t>(Val);
  }
  return 0;
}

} // namespace rcc