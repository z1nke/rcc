#include "Sema/Sema.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Type.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceLocation.h"
#include "Sema/DeclSpec.h"

#include <algorithm>
#include <ranges>

namespace rcc {

Decl *Sema::actOnDeclarator(Declarator &D) {
  QualType T = getTypeForDeclarator(D);

  if (const auto *FT = dyn_cast<FunctionType>(T))
    return actOnFunctionDecl(D, FT, nullptr);

  return actOnVarDecl(D, T);
}

VarDecl *Sema::actOnVarDecl(Declarator &D, QualType T) {
  const DeclSpec &DS = D.getDeclSpec();
  VarDecl *Var = VarDecl::create(Ctx, D.getLocation(), DS.getTypeSpecLoc(),
                                 D.getEndLoc(), T, D.getIdent());
  LocalVars.push_back(Var);
  CurrScope->addDecl(Var);
  return Var;
}

ParamVarDecl *Sema::actOnParamVarDecl(Declarator &D, unsigned Index) {
  assert(CurrScope->getFlags() & Scope::FnScope);
  QualType T = getTypeForDeclarator(D);
  const DeclSpec &DS = D.getDeclSpec();
  ParamVarDecl *Param =
      ParamVarDecl::create(Ctx, D.getLocation(), DS.getTypeSpecLoc(),
                           D.getEndLoc(), T, D.getIdent(), Index);
  Params.push_back(Param);
  CurrScope->addDecl(Param);
  return Param;
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

  return FunctionDecl::create(Ctx, Loc, BegLoc, EndLoc,
                              Ctx.getFunctionType(RetType, {}), std::move(Name),
                              Body);
}

void Sema::complete(FunctionDecl *FD) {
  std::vector<VarDecl *> Vars;
  std::swap(Vars, LocalVars);
  std::ranges::reverse(Vars);
  FD->setLocalVars(std::move(Vars));

  std::vector<QualType> ParamTypes;
  for (const auto *Param : Params)
    ParamTypes.push_back(Param->getType());

  std::vector<ParamVarDecl *> PVars;
  std::swap(PVars, Params);
  FD->setParams(std::move(PVars));
  QualType FT = FD->getType();
  const auto *FuncTy = dyn_cast<FunctionType>(FT);
  if (!FuncTy)
    Diag.fatalAt(FD->getLocation(), "expect function type");

  QualType RetType = FuncTy->getReturnType();

  QualType NewFT = Ctx.getFunctionType(RetType, std::move(ParamTypes));
  FD->setType(NewFT);
  Funcs.push_back(FD);
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
  // TODO: Check return value type and function return type.
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
  QualType ResType = checkBinaryOperatorType(OpLoc, LHS, RHS, Op);
  auto BegLoc = LHS->getBeginLoc();
  auto EndLoc = RHS->getEndLoc();
  return BinaryOperator::create(Ctx, BegLoc, EndLoc, ResType, OpLoc, LHS, RHS,
                                static_cast<BinaryOperator::Opcode>(Op));
}

Expr *Sema::actOnUnaryOperator(SourceLocation OpLoc, Expr *SubExpr,
                               unsigned Op) {
  QualType ResType = checkUnaryOperatorType(OpLoc, SubExpr, Op);
  return UnaryOperator::create(Ctx, OpLoc, SubExpr->getEndLoc(), ResType,
                               SubExpr, static_cast<UnaryOperator::Opcode>(Op));
}

Expr *Sema::actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc, Expr *Ex) {
  // FIXME: Fix sizeof type, int -> size_t.
  return UnaryExprOrTypeTraitExpr::create(Ctx, BegLoc, Ex->getEndLoc(),
                                          Ctx.IntTy, Ex);
}

Expr *Sema::actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                          SourceLocation EndLoc, Type *Ty) {
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
    // Implicit function declaration.
    FD = FunctionDecl::create(
        Ctx, SourceLocation(), SourceLocation(), SourceLocation(),
        Ctx.getFunctionType(Ctx.IntTy, {}), std::string(Name), nullptr);
    Funcs.push_back(FD);
    FD->setImplicit(true);
  }

  auto *Ref = DeclRefExpr::create(Ctx, IdentBegLoc, IdentEndLoc, Ctx.IntTy, FD);
  return CallExpr::create(Ctx, IdentBegLoc, EndLoc, Ctx.IntTy, Ref,
                          std::move(Args));
}

Expr *Sema::actOnStmtExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                          Stmt *SubStmt) {
  auto *CS = dyn_cast<CompoundStmt>(SubStmt);
  if (!CS)
    Diag.fatalAt(BegLoc, "statement expression requires compound statement");

  const auto &Body = CS->getBody();
  if (Body.empty())
    Diag.fatalAt(BegLoc, "statement expression requires non-empty body");

  const auto *Back = dyn_cast<Expr>(Body.back());
  if (!Back)
    Diag.fatalAt(Back->getBeginLoc(), "expected expression");

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

QualType Sema::getCommonArithmeticType(QualType LType, QualType RType) {
  return LType;
}

bool Sema::canCast(QualType LType, QualType RType) {
  bool LIsPtr = LType->isPointerType();
  bool RIsPtr = RType->isPointerType();
  if (LIsPtr && RIsPtr) {
    return canCast(cast<PointerType>(LType)->getPointeeType(),
                   cast<PointerType>(RType)->getPointeeType());
  }

  bool LIsArithmetic = LType->isArithmeticType();
  bool RIsArithmetic = LType->isArithmeticType();
  if (LIsArithmetic && RIsArithmetic)
    return true;

  return false;
}

QualType Sema::checkBinaryOperatorType(SourceLocation OpLoc, Expr *LHS,
                                       Expr *RHS, unsigned Op) {
  switch (Op) {
  case BinaryOperator::BO_Assign: {
    QualType LType = LHS->getType();
    if (LType->isArraryType())
      Diag.fatalAt(LHS->getBeginLoc(), "cannot assign to array type");

    QualType RType = RHS->getType();
    // FIXME: Check LHS type.
    if (LType.isNull()) {
      LType = RType;
      LHS->setType(LType);
    } else {
      if (!canCast(LType, RType))
        Diag.fatalAt(OpLoc, "invalid operand");
    }

    return LType;
  }
  case BinaryOperator::BO_Add: {
    QualType LType = tryDecayArrayType(LHS->getType());
    QualType RType = tryDecayArrayType(RHS->getType());
    checkScalarType(LType);
    checkScalarType(RType);
    bool LIsPtr = LType->isPointerType();
    bool RIsPtr = RType->isPointerType();
    if (LIsPtr && RIsPtr)
      Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");

    bool LIsArithmetic = LType->isArithmeticType();
    bool RIsArithmetic = LType->isArithmeticType();
    if (LIsArithmetic && RIsArithmetic)
      return getCommonArithmeticType(LType, RType);

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
  case BinaryOperator::BO_Sub: {
    QualType LType = tryDecayArrayType(LHS->getType());
    QualType RType = tryDecayArrayType(RHS->getType());
    checkScalarType(LType);
    checkScalarType(RType);
    bool LIsPtr = LType->isPointerType();
    bool RIsPtr = RType->isPointerType();
    if (LIsPtr && RIsPtr)
      return Ctx.IntTy; // FIXME: ptrdiff_t

    bool LIsArithmetic = LType->isArithmeticType();
    bool RIsArithmetic = LType->isArithmeticType();
    if (LIsArithmetic && RIsArithmetic)
      return getCommonArithmeticType(LType, RType);

    if (LIsPtr) {
      checkIntType(RHS);
      return LType;
    }

    Diag.fatalAt(OpLoc, "invalid operand");
  }
  case BinaryOperator::BO_Mul:
  case BinaryOperator::BO_Div: {
    // FIXME: Only int type.
    if (LHS->getType()->isPointerType())
      Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");
    if (RHS->getType()->isPointerType())
      Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");
    return Ctx.IntTy;
  }
  case BinaryOperator::BO_EQ:
  case BinaryOperator::BO_NE:
  case BinaryOperator::BO_LT:
  case BinaryOperator::BO_GT:
  case BinaryOperator::BO_LE:
  case BinaryOperator::BO_GE:
    // TODO: Check operands and add implicit expr.
    return Ctx.IntTy;
  default:
    Diag.fatalAt(OpLoc, "unknown binary opcode");
  }
}

QualType Sema::checkUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
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
    if (const auto *PtrType = SubExpr->getType()->getAs<PointerType>())
      return PtrType->getPointeeType();

    if (const auto *ArrType = SubExpr->getType()->getAs<ArrayType>())
      return ArrType->getElementType();

    Diag.fatalAt(SubExpr->getBeginLoc(),
                 "dereference requires pointer operand");
  }
  default:
    Diag.fatalAt(OpLoc, "unknown unary opcode");
  }
}

VarDecl *Sema::findVar(std::string_view Ident) {
  for (Scope *S = CurrScope; S; S = S->getParent()) {
    for (auto *D : S->decls()) {
      auto *Var = dyn_cast<VarDecl>(D);
      if (!Var)
        continue;

      if (Var->getName() == Ident)
        return Var;
    }
  }

  return nullptr;
}

FunctionDecl *Sema::findFunction(std::string_view Name) {
  for (FunctionDecl *FD : Funcs) {
    if (FD->getName() == Name)
      return FD;
  }

  return nullptr;
}

QualType Sema::getTypeForDeclarator(Declarator &D) {
  QualType T;
  const DeclSpec &DS = D.getDeclSpec();
  switch (DS.getTypeSpecType()) {
  case DeclSpec::TST_Int:
    T = Ctx.IntTy;
    break;
  case DeclSpec::TST_Char:
    T = Ctx.CharTy;
    break;
  default:
    Diag.fatalAt(DS.getTypeSpecLoc(), "unknown type specifier");
  }

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
  if (const auto *IL = dyn_cast<IntegerLiteral>(E)) {
    std::int64_t Val = IL->getVal();
    if (Val <= 0)
      Diag.fatalAt(IL->getBeginLoc(), "array size must be positive");
    return static_cast<std::size_t>(Val);
  }
  return 0;
}

} // namespace rcc