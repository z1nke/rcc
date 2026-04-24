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

static bool isCharArrayType(const ASTContext &Ctx, QualType T) {
  const auto *CAT = T->getAs<ConstantArrayType>();
  if (!CAT)
    return false;
  return Ctx.hasSameType(CAT->getElementType(), Ctx.CharTy);
}

static void checkStringLiteralInit(const ASTContext &Ctx, Diagnostic &Diag,
                                   QualType ArrTy, const StringLiteral *SL) {
  const auto *CAT = ArrTy->getAs<ConstantArrayType>();
  if (!CAT || !Ctx.hasSameType(CAT->getElementType(), Ctx.CharTy))
    Diag.fatalAt(SL->getBeginLoc(), "invalid variable init type");
  if (SL->getString().size() > CAT->getLength())
    Diag.fatalAt(SL->getBeginLoc(), "initializer-string for char array is too long");
}

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
  if (T->isArraryType()) {
    // TODO: Add DecayedType.
    QualType BaseType = T->getBaseElementType();
    T = Ctx.getPointerType(BaseType);
  }

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

  return actOnFunctionDecl(Ctx, D.getDeclSpec(), D.getLocation(),
                           D.getTypeSpecLoc(), D.getEndLoc(), D.getIdent(),
                           FT->getReturnType(), Body);
}

FunctionDecl *Sema::actOnFunctionDecl(ASTContext &Ctx, const DeclSpec &DS,
                                      SourceLocation Loc, SourceLocation BegLoc,
                                      SourceLocation EndLoc, std::string Name,
                                      QualType RetType, Stmt *Body) {
  auto *Func = FunctionDecl::create(Ctx, Loc, BegLoc, EndLoc,
                                    Ctx.getFunctionType(RetType, {}),
                                    std::move(Name), Body);
  assert(CurrScope->isFunctionScope());
  addDecl(Func);
  if (DS.getStorageClassSpec() == DeclSpec::SCS_Static)
    Func->setIsStatic();
  Funcs.push_back(Func);
  return Func;
}

TagDecl *Sema::actOnTagDecl(SourceLocation Loc, SourceLocation BegLoc,
                            SourceLocation EndLoc, std::string_view Ident,
                            unsigned TagKind) {
  auto TK = static_cast<TagDecl::TagKind>(TagKind);
  TagDecl *Tag = nullptr;
  switch (TK) {
  case TagDecl::TK_Struct:
  case TagDecl::TK_Union: {
    auto *Record =
        RecordDecl::create(Ctx, Loc, BegLoc, EndLoc, std::string(Ident),
                           static_cast<RecordDecl::TagKind>(TagKind));
    Tag = Record;
    QualType RT = Ctx.getRecordType(Record, /*Size=*/0, /*Align=*/1);
    Tag->setTypeForDecl(RT.getTypePtr());
    break;
  }
  case TagDecl::TK_Enum:
    auto *Enum = EnumDecl::create(Ctx, Loc, BegLoc, EndLoc, std::string(Ident));
    Tag = Enum;
    QualType ET = Ctx.getEnumType(Enum);
    Tag->setTypeForDecl(ET.getTypePtr());
    break;
  }

  return Tag;
}

void Sema::actOnTagStartDefinition(SourceLocation Loc, TagDecl *Tag) {
  std::string_view Ident = Tag->getName();
  unsigned TagKind = Tag->getTagKind();
  TagDecl *FirstDecl = nullptr;
  if (!Ident.empty()) {
    for (auto *Tag : std::views::reverse(CurrScope->tags())) {
      if (Tag->getName() != Ident)
        continue;
      if (Tag->getTagKind() != static_cast<TagDecl::TagKind>(TagKind))
        Diag.fatalAt(Loc, "redefinition of '{}' with wrong kind", Ident);
      if (Tag->getDefinition())
        actOnDuplicateDefinition(Loc, Ident, TagKind);
      FirstDecl = Tag->getCanonicalDecl();
      break;
    }
  }

  Tag->setCanonicalDecl(FirstDecl ? FirstDecl : Tag);
  Tag->setDefinition(Tag);
}

void Sema::actOnTagFinishDefinition(TagDecl *Tag, SourceLocation EndLoc) {
  if (auto *Record = dynCast<RecordDecl>(Tag)) {
    std::size_t Size = 0;
    std::size_t Align = 1;
    if (Record->isStruct()) {
      std::size_t Offset = 0;
      for (auto *Field : Record->fields()) {
        std::size_t FieldAlign = Field->getType()->getAlign();
        Offset = alignTo(Offset, FieldAlign);
        Field->setOffset(Offset);
        Offset += Field->getType()->getSize();
        if (Align < FieldAlign)
          Align = FieldAlign;
      }
      Size = alignTo(Offset, Align);
    } else {
      for (auto *Field : Record->fields()) {
        std::size_t FieldAlign = Field->getType()->getAlign();
        std::size_t FieldSize = Field->getType()->getSize();
        if (Align < FieldAlign)
          Align = FieldAlign;
        if (Size < FieldSize)
          Size = FieldSize;
      }
      Size = alignTo(Size, Align);
    }

    QualType RT = Ctx.getRecordType(Record, Size, Align);
    auto *RTTy = const_cast<Type *>(RT.getTypePtr());
    RTTy->setSize(Size);
    RTTy->setAlign(Align);
    Record->setTypeForDecl(RT.getTypePtr());
  }

  Tag->setEndLoc(EndLoc);
  if (Tag->getName().empty())
    return;

  for (auto *Prev : CurrScope->tags()) {
    if (Prev == Tag)
      continue;
    if (Prev->getName() != Tag->getName())
      continue;
    if (Prev->getTagKind() != Tag->getTagKind())
      continue;
    assert(!Prev->getDefinition());
    Prev->setDefinition(Tag);
  }
}

EnumConstantDecl *
Sema::actOnEnumConstantDecl(SourceLocation Loc, SourceLocation BegLoc,
                            SourceLocation EndLoc, QualType T, std::string Name,
                            std::int64_t Val, const Expr *Init) {
  auto *ECD = EnumConstantDecl::create(Ctx, Loc, BegLoc, EndLoc, T,
                                       std::move(Name), Val, Init);
  addDecl(ECD);
  return ECD;
}

void Sema::actOnDuplicateDefinition(SourceLocation Loc, std::string_view Name,
                                    unsigned TagKind) const {
  switch (static_cast<TagDecl::TagKind>(TagKind)) {
  case TagDecl::TK_Struct:
    Diag.fatalAt(Loc, "redefinition of struct '{}'", Name);
  case TagDecl::TK_Union:
    Diag.fatalAt(Loc, "redefinition of union '{}'", Name);
  case TagDecl::TK_Enum:
    Diag.fatalAt(Loc, "redefinition of enum '{}'", Name);
  }
  RCC_UNREACHABLE("Unknown tag kind");
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

  if (!FD->getBody()) {
    Labels.clear();
    return;
  }

  for (const auto &[Name, Label] : Labels) {
    if (!Label->getStmt())
      Diag.fatalAt(FD->getBeginLoc(), "use of undeclared label '{}'", Name);
  }
  Labels.clear();

  std::vector<VarDecl *> Vars;
  std::swap(Vars, LocalVars);
  std::ranges::reverse(Vars);
  FD->setLocalVars(std::move(Vars));
}

void Sema::complete(VarDecl *Var, Expr *Init) {
  QualType InitType = Init->getType();
  QualType VarType = Var->getType();
  if (const auto *IAT = VarType->getAs<IncompleteArrayType>()) {
    QualType ElemTy = IAT->getElementType();
    if (const auto *SL = dynCast<StringLiteral>(Init)) {
      if (!Ctx.hasSameType(ElemTy, Ctx.CharTy))
        Diag.fatalAt(Var->getLocation(), "invalid variable init type");
      VarType = Ctx.getConstantArrayType(ElemTy, SL->getString().size() + 1);
      Var->setType(VarType);
    } else if (const auto *ILE = dynCast<InitListExpr>(Init)) {
      if (ILE->getNumInits() == 0)
        Diag.fatalAt(Init->getBeginLoc(), "array size must be positive");
      VarType = Ctx.getConstantArrayType(ElemTy, ILE->getNumInits());
      Var->setType(VarType);
    }
  }

  if (const auto *SL = dynCast<StringLiteral>(Init)) {
    if (!isCharArrayType(Ctx, VarType))
      Diag.fatalAt(Var->getLocation(), "invalid variable init type");
    checkStringLiteralInit(Ctx, Diag, VarType, SL);
    Var->setInit(Init);
    Var->setEndLoc(Init->getEndLoc());
    return;
  }

  if (const auto *ILE = dynCast<InitListExpr>(Init)) {
    if (!VarType->isArraryType() && !VarType->isRecordType())
      Diag.fatalAt(Init->getBeginLoc(), "invalid initializer list for scalar");
    checkInitList(ILE, VarType);
    Var->setInit(Init);
    Var->setEndLoc(Init->getEndLoc());
    return;
  }

  if (!Ctx.hasSameType(VarType, InitType)) {
    auto CK = getCastKind(VarType, InitType);
    if (!CK)
      Diag.fatalAt(Var->getLocation(), "invalid variable init type");

    if (*CK != CastExpr::CK_NoOp)
      Init = impCastExprToType(Init, VarType, *CK);
  }

  Var->setInit(Init);
  Var->setEndLoc(Init->getEndLoc());
}

void Sema::checkInitList(const InitListExpr *List, QualType AggTy) const {
  if (const auto *CAT = AggTy->getAs<ConstantArrayType>()) {
    QualType ElemTy = CAT->getElementType();
    unsigned NumToCheck =
        std::min<unsigned>(List->getNumInits(), CAT->getLength());
    for (unsigned I = 0; I < NumToCheck; ++I)
      checkInitListElement(List->getInit(I), ElemTy);
    return;
  }

  if (const auto *RT = AggTy->getAs<RecordType>()) {
    const auto *RD = RT->getDecl();
    const auto &Fields = RD->fields();
    unsigned NumToCheck =
        std::min<unsigned>(List->getNumInits(), Fields.size());
    for (unsigned I = 0; I < NumToCheck; ++I)
      checkInitListElement(List->getInit(I), Fields[I]->getType());
    return;
  }

  Diag.fatalAt(List->getBeginLoc(), "expect aggregate type");
}

void Sema::checkInitListElement(const Expr *E, QualType ElemTy) const {
  if (const auto *SubList = dynCast<InitListExpr>(E)) {
    if (!ElemTy->isArraryType() && !ElemTy->isRecordType())
      Diag.fatalAt(SubList->getBeginLoc(), "invalid nested initializer list");
    checkInitList(SubList, ElemTy);
    return;
  }

  if (ElemTy->isArraryType()) {
    if (const auto *SL = dynCast<StringLiteral>(E)) {
      checkStringLiteralInit(Ctx, Diag, ElemTy, SL);
      return;
    }
    Diag.fatalAt(E->getBeginLoc(), "expect nested initializer list");
  }

  if (ElemTy->isRecordType())
    Diag.fatalAt(E->getBeginLoc(), "expect nested initializer list");

  auto CK = getCastKind(ElemTy, E->getType());
  if (!CK)
    Diag.fatalAt(E->getBeginLoc(), "invalid variable init type");
}

Expr *Sema::actOnCharacterLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                                  QualType T, unsigned Val) {
  return CharacterLiteral::create(Ctx, BegLoc, EndLoc, T, Val);
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
  checkScalarType(Cond);
  auto EndLoc = Else ? Else->getEndLoc() : Then->getEndLoc();
  return IfStmt::create(Ctx, BegLoc, EndLoc, Cond, Then, Else);
}

Stmt *Sema::actOnForStmt(SourceLocation BegLoc, Stmt *Init, Expr *Cond,
                         Expr *Inc, Stmt *Body) {
  if (Cond)
    checkScalarType(Cond);
  auto EndLoc = Body->getEndLoc();
  return ForStmt::create(Ctx, BegLoc, EndLoc, Init, Cond, Inc, Body);
}

Stmt *Sema::actOnWhileStmt(ASTContext &Ctx, SourceLocation BegLoc, Expr *Cond,
                           Stmt *Body) {
  checkScalarType(Cond);
  auto EndLoc = Body->getEndLoc();
  return WhileStmt::create(Ctx, BegLoc, EndLoc, Cond, Body);
}

void Sema::actOnSwitchStmtStart() { SwitchStack.push_back({}); }

Stmt *Sema::actOnSwitchStmt(SourceLocation BegLoc, Expr *Cond, Stmt *Body) {
  if (SwitchStack.empty())
    Diag.fatalAt(BegLoc, "internal error: missing switch context");

  checkIntType(Cond);
  SwitchInfo SI = std::move(SwitchStack.back());
  SwitchStack.pop_back();
  auto EndLoc = Body->getEndLoc();
  return SwitchStmt::create(Ctx, BegLoc, EndLoc, Cond, Body, SI.FirstCase);
}

Stmt *Sema::actOnCaseStmt(SourceLocation BegLoc, Expr *LHS, Stmt *SubStmt) {
  if (SwitchStack.empty())
    Diag.fatalAt(BegLoc, "'case' statement not in switch statement");

  checkIntType(LHS);
  auto Val = LHS->evaluateAsInt();
  if (!Val)
    Diag.fatalAt(BegLoc, "case label does not reduce to an integer constant");

  std::int64_t CaseValue = *Val;

  SwitchInfo &SI = SwitchStack.back();
  for (const auto *SC = SI.FirstCase; SC; SC = SC->getNextSwitchCase()) {
    const auto *CS = dynCast<CaseStmt>(SC);
    if (CS && CS->getCaseValue() == CaseValue)
      Diag.fatalAt(BegLoc, "duplicate case value");
  }

  auto EndLoc = SubStmt->getEndLoc();
  auto LabelId = SI.NextLabelId++;
  auto *CS = CaseStmt::create(Ctx, BegLoc, EndLoc, LHS, SubStmt, CaseValue,
                              LabelId);
  CS->setNextSwitchCase(SI.FirstCase);
  SI.FirstCase = CS;
  return CS;
}

Stmt *Sema::actOnDefaultStmt(SourceLocation BegLoc, Stmt *SubStmt) {
  if (SwitchStack.empty())
    Diag.fatalAt(BegLoc, "'default' statement not in switch statement");

  SwitchInfo &SI = SwitchStack.back();
  if (SI.HasDefault)
    Diag.fatalAt(BegLoc, "multiple default labels in one switch");

  auto EndLoc = SubStmt->getEndLoc();
  auto LabelId = SI.NextLabelId++;
  auto *DS = DefaultStmt::create(Ctx, BegLoc, EndLoc, SubStmt, LabelId);
  DS->setNextSwitchCase(SI.FirstCase);
  SI.FirstCase = DS;
  SI.HasDefault = true;
  return DS;
}

Stmt *Sema::actOnBreakStmt(SourceLocation BegLoc, SourceLocation EndLoc) {
  Scope *S = CurrScope;
  while (S && !(S->getFlags() & Scope::BreakScope))
    S = S->getParent();

  if (!S)
    Diag.fatalAt(BegLoc, "break statement not in loop or switch statement");

  return BreakStmt::create(Ctx, BegLoc, EndLoc);
}

Stmt *Sema::actOnContinueStmt(SourceLocation BegLoc, SourceLocation EndLoc) {
  Scope *S = CurrScope;
  while (S && !(S->getFlags() & Scope::ContinueScope))
    S = S->getParent();

  if (!S)
    Diag.fatalAt(BegLoc, "continue statement not in loop statement");

  return ContinueStmt::create(Ctx, BegLoc, EndLoc);
}

Stmt *Sema::actOnGotoStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                          std::string_view LabelName) {
  auto Iter = Labels.find(std::string(LabelName));
  LabelDecl *Label = nullptr;
  if (Iter != Labels.end()) {
    Label = Iter->second;
  } else {
    Label =
        LabelDecl::create(Ctx, BegLoc, BegLoc, EndLoc, std::string(LabelName));
    Labels.emplace(Label->getName(), Label);
  }
  return GotoStmt::create(Ctx, BegLoc, EndLoc, Label);
}

Stmt *Sema::actOnLabelStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                           std::string_view LabelName, Stmt *SubStmt) {
  auto Iter = Labels.find(std::string(LabelName));
  LabelDecl *Label = nullptr;
  if (Iter != Labels.end()) {
    Label = Iter->second;
    if (Label->getStmt())
      Diag.fatalAt(BegLoc, "duplicate label '{}'", LabelName);
    Label->setBeginLoc(BegLoc);
    Label->setEndLoc(EndLoc);
  } else {
    Label =
        LabelDecl::create(Ctx, BegLoc, BegLoc, EndLoc, std::string(LabelName));
    Labels.emplace(Label->getName(), Label);
  }
  auto *LS = LabelStmt::create(Ctx, BegLoc, EndLoc, Label, SubStmt);
  Label->setStmt(LS);
  return LS;
}

Expr *Sema::actOnBinaryOperator(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                                unsigned Op) {
  QualType ResType = getBinaryOperatorType(OpLoc, LHS, RHS, Op);
  auto BegLoc = LHS->getBeginLoc();
  auto EndLoc = RHS->getEndLoc();
  return BinaryOperator::create(Ctx, BegLoc, EndLoc, ResType, OpLoc, LHS, RHS,
                                static_cast<BinaryOperator::Opcode>(Op));
}

Expr *Sema::actOnConditionalOperator(SourceLocation QLoc, SourceLocation ColonLoc,
                                     Expr *Cond, Expr *TrueExpr,
                                     Expr *FalseExpr) {
  (void)ColonLoc;
  Cond = usualUnaryConv(Cond);
  checkScalarType(Cond);

  QualType ResType = getConditionalOperatorType(QLoc, TrueExpr, FalseExpr);
  return ConditionalOperator::create(Ctx, Cond->getBeginLoc(),
                                     FalseExpr->getEndLoc(), ResType, Cond,
                                     TrueExpr, FalseExpr);
}

Expr *Sema::actOnUnaryOperator(SourceLocation OpLoc, Expr *SubExpr,
                               unsigned Op) {
  QualType ResType = getUnaryOperatorType(OpLoc, SubExpr, Op);
  return UnaryOperator::create(Ctx, OpLoc, SubExpr->getEndLoc(), ResType,
                               SubExpr, static_cast<UnaryOperator::Opcode>(Op));
}

Expr *Sema::actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc, Expr *Ex) {
  checkSizeofType(BegLoc, Ex->getType());
  // FIXME: Fix sizeof type, int -> size_t.
  return UnaryExprOrTypeTraitExpr::create(Ctx, BegLoc, Ex->getEndLoc(),
                                          Ctx.IntTy, Ex);
}

Expr *Sema::actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                          SourceLocation EndLoc,
                                          const Type *Ty) {
  checkSizeofType(BegLoc, QualType(Ty));
  // FIXME: Fix sizeof type, int -> size_t.
  return UnaryExprOrTypeTraitExpr::create(Ctx, BegLoc, EndLoc, Ctx.IntTy, Ty);
}

Expr *Sema::actOnParenExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                           Expr *SubExpr) {
  return ParenExpr::create(Ctx, BegLoc, EndLoc, SubExpr->getType(), SubExpr);
}

Expr *Sema::actOnDeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                             std::string_view Ident) {
  auto *ND = findValueDecl(Ident);
  if (!ND)
    Diag.fatalAt(BegLoc, "undeclared variable '{}'", Ident.data());

  return DeclRefExpr::create(Ctx, BegLoc, EndLoc, ND->getType(), ND);
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
  unsigned NumArgs = Args.size();
  unsigned NumParams = FD->getNumParams();
  unsigned N = std::min(NumArgs, NumParams);
  for (unsigned I = 0; I < N; ++I) {
    Expr *Arg = Args[I];
    Arg = usualUnaryConv(Arg);
    QualType ArgType = Arg->getType();
    QualType ParamType = FT->getParamType(I);
    if (!Ctx.hasSameType(ParamType, ArgType)) {
      auto CK = getCastKind(ParamType, ArgType);
      if (!CK)
        Diag.fatalAt(Arg->getBeginLoc(), "invalid argument type");

      if (*CK != CastExpr::CK_NoOp)
        Arg = impCastExprToType(Arg, ParamType, *CK);
    }
    Args[I] = Arg;
  }

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

  if (!Record->hasDefinition())
    Diag.fatalAt(BegLoc, "incomplete definition of type");

  // TODO: Record type must be complete.
  for (auto &Field : Record->fields()) {
    if (Field->getName() != Ident)
      continue;
    return MemberExpr::create(Ctx, BegLoc, OpLoc, EndLoc, Field->getType(),
                              Base, Field, IsArrow);
  }

  Diag.fatalAt(BegLoc, "field '{}' not found in record", Ident);
}

void Sema::checkScalarType(Expr *E) const {
  if (!E->getType()->isScalarType())
    Diag.fatalAt(E->getBeginLoc(), "type requires scalar type");
}

void Sema::checkIntType(Expr *E) const {
  if (!E->getType().isIntegerType())
    Diag.fatalAt(E->getBeginLoc(), "expression requires integer type");
}

void Sema::checkArithmeticType(Expr *E) const {
  if (!E->getType()->isArithmeticType())
    Diag.fatalAt(E->getBeginLoc(), "expression requires arithmetic type");
}

void Sema::checkSizeofType(SourceLocation BegLoc, QualType T) const {
  if (T->isFunctionType())
    Diag.fatalAt(BegLoc, "invalid application of 'sizeof' to a function type");

  if (T->isIncompleteType()) {
    Diag.fatalAt(BegLoc,
                 "invalid application of 'sizeof' to a incomplete type '{}'",
                 T.getAsString());
  }
}

/// usualUnaryConv - Performs various conversions that are common to most
/// operators (C99 6.3). The conversions of array and function types are
/// sometimes suppressed. For example, the array->pointer conversion doesn't
/// apply if the array is an argument to the sizeof or address (&) operators.
/// In these instances, this routine should *not* be called.
Expr *Sema::usualUnaryConv(Expr *E) const {
  E = defaultFunctionArrayLvalueConv(E);
  assert(E);

  // TODO: Try to perform integral promotions if the object has a theoretically
  // promotable type.
  // ...

  return E;
}

Expr *Sema::defaultFunctionArrayLvalueConv(Expr *E) const {
  return defaultLvalueConv(defaultFunctionArrayConv(E));
}

Expr *Sema::defaultFunctionArrayConv(Expr *E) const {
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

Expr *Sema::defaultLvalueConv(Expr *E) const {
  // TODO: Impl
  return E;
}

using PerformCastFn = Expr *(*)(const Sema &, Expr *, QualType);

template <PerformCastFn doLHSCast, PerformCastFn doRHSCast>
static QualType handleArithConv(const Sema &S, Expr *&LHS, Expr *&RHS,
                                QualType LType, QualType RType,
                                bool IsCompAssign) {
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

static Expr *doIntegralCast(const Sema &S, Expr *E, QualType ToType) {
  return S.impCastExprToType(E, ToType, CastExpr::CK_IntegralCast);
}

/// usualArithConv - Performs various conversions that are common to
/// binary operators (C99 6.3.1.8). If both operands aren't arithmetic, this
/// routine returns the first non-arithmetic type found. The client is
/// responsible for emitting appropriate error diagnostics.
QualType Sema::usualArithConv(Expr *&LHS, Expr *&RHS, ArithConvKind ACK) const {
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

Expr *Sema::impCastExprToType(Expr *E, QualType Ty, unsigned CK) const {
  QualType ExprTy = E->getType().getCanonicalType();
  QualType TypeTy = Ty.getCanonicalType();
  if (ExprTy == TypeTy)
    return E;

  return CastExpr::create(Ctx, E->getBeginLoc(), E->getEndLoc(), Ty, E,
                          static_cast<CastExpr::CastKind>(CK),
                          true /*Implicit*/);
}

std::optional<unsigned> Sema::getCastKind(QualType ToType,
                                          QualType FromType) const {
  if (ToType == FromType)
    return CastExpr::CK_NoOp;

  const auto *ToPtrTy = ToType->getAs<PointerType>();
  const auto *FromPtrTy = FromType->getAs<PointerType>();
  if (FromType->isArraryType() && ToPtrTy)
    return CastExpr::CK_ArrayToPointerDecay;

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

static bool isNullPtrConstExpr(const Expr *E) {
  E = E->ignoreParenCasts();
  if (const auto *IL = dynCast<IntegerLiteral>(E))
    return IL->getVal() == 0;
  return false;
}

QualType Sema::getCompoundAssignOpType(SourceLocation OpLoc, Expr *&LHS,
                                       Expr *&RHS, unsigned Op) const {
  QualType LType = LHS->getType();
  if (LType->isArraryType())
    Diag.fatalAt(LHS->getBeginLoc(), "cannot assign to array type");

  switch (Op) {
  case BinaryOperator::BO_AddAssign:
    (void)getAddOpType(OpLoc, LHS, RHS, true);
    break;
  case BinaryOperator::BO_SubAssign:
    (void)getSubOpType(OpLoc, LHS, RHS, true);
    break;
  case BinaryOperator::BO_MulAssign:
  case BinaryOperator::BO_DivAssign:
  case BinaryOperator::BO_RemAssign:
    (void)getMulDivOpType(OpLoc, LHS, RHS, true);
    break;
  case BinaryOperator::BO_AndAssign:
  case BinaryOperator::BO_OrAssign:
  case BinaryOperator::BO_XorAssign:
    (void)getBitwiseOpType(OpLoc, LHS, RHS, true);
    break;
  case BinaryOperator::BO_ShlAssign:
  case BinaryOperator::BO_ShrAssign:
    (void)getShiftOpType(OpLoc, LHS, RHS, true);
    break;
  default:
    Diag.fatalAt(OpLoc, "unknown compound assignment opcode");
  }

  return LType;
}

QualType Sema::getConditionalOperatorType(SourceLocation OpLoc, Expr *&TrueExpr,
                                          Expr *&FalseExpr) const {
  TrueExpr = defaultFunctionArrayLvalueConv(TrueExpr);
  FalseExpr = defaultFunctionArrayLvalueConv(FalseExpr);

  QualType TType = TrueExpr->getType();
  QualType FType = FalseExpr->getType();
  if (Ctx.hasSameType(TType, FType))
    return TType;

  if (TType.isVoidType() || FType.isVoidType()) {
    if (!TType.isVoidType())
      TrueExpr = impCastExprToType(TrueExpr, Ctx.VoidTy, CastExpr::CK_ToVoid);
    if (!FType.isVoidType())
      FalseExpr = impCastExprToType(FalseExpr, Ctx.VoidTy, CastExpr::CK_ToVoid);
    return Ctx.VoidTy;
  }

  if (TType->isArithmeticType() && FType->isArithmeticType())
    return usualArithConv(TrueExpr, FalseExpr, ACK_Conditional);

  if (TType->isPointerType() && FType->isPointerType()) {
    auto CK = getCastKind(TType, FType);
    if (CK && *CK != CastExpr::CK_NoOp)
      FalseExpr = impCastExprToType(FalseExpr, TType, *CK);
    return TType;
  }

  if (TType->isPointerType() && isNullPtrConstExpr(FalseExpr)) {
    FalseExpr =
        impCastExprToType(FalseExpr, TType, CastExpr::CK_IntegralToPointer);
    return TType;
  }

  if (FType->isPointerType() && isNullPtrConstExpr(TrueExpr)) {
    TrueExpr =
        impCastExprToType(TrueExpr, FType, CastExpr::CK_IntegralToPointer);
    return FType;
  }

  Diag.fatalAt(OpLoc, "invalid conditional operands");
}

QualType Sema::getBinaryOperatorType(SourceLocation OpLoc, Expr *&LHS,
                                     Expr *&RHS, unsigned Op) const {
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
  case BinaryOperator::BO_Rem:
    return getMulDivOpType(OpLoc, LHS, RHS);
  case BinaryOperator::BO_And:
  case BinaryOperator::BO_Or:
  case BinaryOperator::BO_Xor:
    return getBitwiseOpType(OpLoc, LHS, RHS);
  case BinaryOperator::BO_Shl:
  case BinaryOperator::BO_Shr:
    return getShiftOpType(OpLoc, LHS, RHS);
  case BinaryOperator::BO_LAnd:
  case BinaryOperator::BO_LOr:
    LHS = usualUnaryConv(LHS);
    RHS = usualUnaryConv(RHS);
    checkScalarType(LHS);
    checkScalarType(RHS);
    return Ctx.IntTy;
  case BinaryOperator::BO_AddAssign:
  case BinaryOperator::BO_SubAssign:
  case BinaryOperator::BO_MulAssign:
  case BinaryOperator::BO_DivAssign:
  case BinaryOperator::BO_RemAssign:
  case BinaryOperator::BO_AndAssign:
  case BinaryOperator::BO_OrAssign:
  case BinaryOperator::BO_XorAssign:
  case BinaryOperator::BO_ShlAssign:
  case BinaryOperator::BO_ShrAssign:
    return getCompoundAssignOpType(OpLoc, LHS, RHS, Op);
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

QualType Sema::getAddOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                            bool IsCompAssign) const {
  if (!IsCompAssign)
    LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);

  checkScalarType(LHS);
  checkScalarType(RHS);
  QualType LType = LHS->getType();
  QualType RType = RHS->getType();
  bool LIsPtr = LType->isPointerType();
  bool RIsPtr = RType->isPointerType();
  if (LIsPtr && RIsPtr)
    Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");

  bool LIsArithmetic = LType->isArithmeticType();
  bool RIsArithmetic = RType->isArithmeticType();
  if (LIsArithmetic && RIsArithmetic)
    return usualArithConv(LHS, RHS,
                          IsCompAssign ? ACK_CompAssign : ACK_Arithmetic);

  if (LIsPtr) {
    checkIntType(RHS);
    return LType;
  }

  if (RIsPtr) {
    if (IsCompAssign)
      Diag.fatalAt(OpLoc, "invalid compound assignment operand");
    checkIntType(LHS);
    return RType;
  }

  Diag.fatalAt(OpLoc, "invalid operand");
}

QualType Sema::getSubOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                            bool IsCompAssign) const {
  if (!IsCompAssign)
    LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);

  checkScalarType(LHS);
  checkScalarType(RHS);
  QualType LType = LHS->getType();
  QualType RType = RHS->getType();
  bool LIsPtr = LType->isPointerType();
  bool RIsPtr = RType->isPointerType();
  if (LIsPtr && RIsPtr) {
    if (IsCompAssign)
      Diag.fatalAt(LHS->getBeginLoc(), "invalid compound assignment operand");
    return Ctx.IntTy; // FIXME: ptrdiff_t
  }

  bool LIsArithmetic = LType->isArithmeticType();
  bool RIsArithmetic = RType->isArithmeticType();
  if (LIsArithmetic && RIsArithmetic)
    return usualArithConv(LHS, RHS,
                          IsCompAssign ? ACK_CompAssign : ACK_Arithmetic);

  if (LIsPtr) {
    checkIntType(RHS);
    return LType;
  }

  Diag.fatalAt(OpLoc, "invalid operand");
}

QualType Sema::getMulDivOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                               bool IsCompAssign) const {
  if (!IsCompAssign)
    LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);
  if (LHS->getType()->isPointerType())
    Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");
  if (RHS->getType()->isPointerType())
    Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");

  auto ACK = IsCompAssign ? ACK_CompAssign : ACK_Arithmetic;
  return usualArithConv(LHS, RHS, ACK);
}

QualType Sema::getBitwiseOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                                bool IsCompAssign) const {
  if (!IsCompAssign)
    LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);

  checkIntType(LHS);
  checkIntType(RHS);
  auto ACK = IsCompAssign ? ACK_CompAssign : ACK_BitwiseOp;
  return usualArithConv(LHS, RHS, ACK);
}

QualType Sema::getShiftOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                              bool IsCompAssign) const {
  if (!IsCompAssign)
    LHS = usualUnaryConv(LHS);
  RHS = usualUnaryConv(RHS);

  checkIntType(LHS);
  checkIntType(RHS);

  (void)OpLoc;
  return LHS->getType();
}

static bool isModifiableLvalue(const Expr *E) {
  E = E->ignoreParens();
  switch (E->getKind()) {
  case Stmt::SK_DeclRefExpr:
  case Stmt::SK_ArraySubscriptExpr:
  case Stmt::SK_MemberExpr:
    return true;
  case Stmt::SK_UnaryOperator:
    return cast<UnaryOperator>(E)->getOpcode() == UnaryOperator::UO_Deref;
  case Stmt::SK_BinaryOperator: {
    const auto *BO = cast<BinaryOperator>(E);
    if (BO->getOpcode() == BinaryOperator::BO_Comma)
      return isModifiableLvalue(BO->getRHS());
    return false;
  }
  case Stmt::SK_ParenExpr:
    return isModifiableLvalue(cast<ParenExpr>(E)->getSubExpr());
  default:
    return false;
  }
}

QualType Sema::getUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
                                    unsigned Op) const {
  switch (Op) {
  case UnaryOperator::UO_Plus:
  case UnaryOperator::UO_Minus:
    checkArithmeticType(SubExpr);
    return SubExpr->getType();
  case UnaryOperator::UO_Not:
    checkIntType(SubExpr);
    // FIXME: Integer promotion.
    return SubExpr->getType();
  case UnaryOperator::UO_LNot:
    checkScalarType(SubExpr);
    return Ctx.IntTy;
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
  case UnaryOperator::UO_PreInc:
  case UnaryOperator::UO_PreDec:
  case UnaryOperator::UO_PostInc:
  case UnaryOperator::UO_PostDec: {
    if (!isModifiableLvalue(SubExpr)) {
      Diag.fatalAt(
          SubExpr->getBeginLoc(), "operand of '{}' must be a modifiable lvalue",
          UnaryOperator::getOpcodeStr(static_cast<UnaryOperator::Opcode>(Op)));
    }

    QualType T = SubExpr->getType();
    if (!T->isScalarType()) {
      Diag.fatalAt(
          SubExpr->getBeginLoc(), "operand of '{}' must have scalar type",
          UnaryOperator::getOpcodeStr(static_cast<UnaryOperator::Opcode>(Op)));
    }
    return T;
  }
  default:
    Diag.fatalAt(OpLoc, "unknown unary opcode");
  }
}

ValueDecl *Sema::findValueDecl(std::string_view Ident) const {
  for (Scope *S = CurrScope; S; S = S->getParent()) {
    for (auto *D : S->decls()) {
      auto *VD = dynCast<ValueDecl>(D);
      if (!VD)
        continue;

      if (VD->getName() == Ident)
        return VD;
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

QualType Sema::convertDeclSpecToType(const DeclSpec &DS) const {
  QualType T;
  switch (DS.getTypeSpecType()) {
  case DeclSpec::TST_Void:
    T = Ctx.VoidTy;
    break;
  case DeclSpec::TST_UnderlineBool:
    T = Ctx.BoolTy;
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
  case DeclSpec::TST_Union:
  case DeclSpec::TST_Enum: {
    const auto *TD = dynCast<TagDecl>(DS.getRepDecl());
    if (!TD)
      Diag.fatalAt(DS.getTypeSpecLoc(), "struct/union has no declaration");
    T = TD->getType();
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

QualType Sema::getTypeForDeclarator(Declarator &D) const {
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
        T = Ctx.getIncompleteArrayType(T);
      else
        T = Ctx.getConstantArrayType(T, getArrayLength(Chunk.Arr.LenExpr));
      break;
    default:
      Diag.fatalAt(DS.getTypeSpecLoc(), "unknown declarator type");
    }
  }

  return T;
}

QualType Sema::tryDecayArrayType(QualType T) const {
  if (const auto *AT = T->getAs<ArrayType>())
    return Ctx.getPointerType(AT->getElementType());
  return T;
}

std::size_t Sema::getArrayLength(const Expr *E) const {
  auto Val = E->evaluateAsInt();
  if (!Val)
    Diag.fatalAt(E->getBeginLoc(),
                 "array size must be an integer constant expression");

  std::int64_t ArrayLen = *Val;
  if (ArrayLen <= 0)
    Diag.fatalAt(E->getBeginLoc(), "array size must be positive");

  return static_cast<std::size_t>(ArrayLen);
}

} // namespace rcc