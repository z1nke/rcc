#include "AST/ASTDumper.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Type.h"
#include "Support/Unreachable.h"

#include <print>

namespace rcc {

void ASTDumper::visit(const Decl *D) {
  if (!D) {
    printNull();
    return;
  }

  switch (D->getKind()) {
  case Decl::DK_TranslationUnit:
    visit(cast<TranslationUnitDecl>(D));
    break;
  case Decl::DK_Function:
    visit(cast<FunctionDecl>(D));
    break;
  case Decl::DK_Var:
    visit(cast<VarDecl>(D));
    break;
  case Decl::DK_Record:
    visit(cast<RecordDecl>(D));
    break;
  case Decl::DK_Field:
    visit(cast<FieldDecl>(D));
    break;
  case Decl::DK_Typedef:
    visit(cast<TypedefDecl>(D));
    break;
  default:
    RCC_UNREACHABLE("unknown decl kind");
  }
}

class ScopedIndent {
public:
  ScopedIndent(ASTDumper &Dumper, bool IsSingle)
      : Dumper(Dumper), IsSingle(IsSingle) {
    Dumper.printIndent();
    std::print(stderr, "{}", IsSingle ? SinglePrefix : MultiPrefix);
    Dumper.indent(IsSingle);
  }
  ~ScopedIndent() { Dumper.unindent(IsSingle); }

  static constexpr const char *SinglePrefix = "`-";
  static constexpr const char *MultiPrefix = "|-";

private:
  ASTDumper &Dumper;
  bool IsSingle;
};

void ASTDumper::visit(const TranslationUnitDecl *TU) {
  printName("TranslationUnitDecl");
  bool IsSingle = TU->decls().size() == 1;
  for (const auto *D : TU->decls()) {
    ScopedIndent SI(*this, IsSingle);
    visit(D);
  }
}

void ASTDumper::visit(const FunctionDecl *Func) {
  printName(Func);
  for (const auto *Param : Func->getParams()) {
    ScopedIndent SI(*this, false);
    visit(Param);
  }

  if (Stmt *Body = Func->getBody()) {
    ScopedIndent SI(*this, false);
    visit(Body);
  }
}

void ASTDumper::visit(const VarDecl *Var) {
  printName(Var);
  if (const Expr *Init = Var->getInit()) {
    ScopedIndent SI(*this, true);
    visit(Init);
  }
}

void ASTDumper::visit(const EnumConstantDecl *ECD) {
  std::println(stderr, "EnumConstantDecl {} {} '{}'", ECD->getName(),
               ECD->getValue(), ECD->getType().getAsString());
  if (ECD->getInit()) {
    ScopedIndent SI(*this, true);
    visit(ECD->getInit());
  }
}

void ASTDumper::visit(const FieldDecl *Field) { printName(Field); }

void ASTDumper::visit(const RecordDecl *Record) {
  printName(Record);
  for (const auto *Field : Record->fields()) {
    ScopedIndent SI(*this, false);
    visit(Field);
  }
}

void ASTDumper::visit(const EnumDecl *Enum) {
  std::println(stderr, "{} {}", Enum->getKindStr(), Enum->getName());
  bool IsSingle = Enum->enumerators().size() == 1;
  for (const auto *ECD : Enum->enumerators()) {
    ScopedIndent SI(*this, IsSingle);
    visit(ECD);
  }
}

void ASTDumper::visit(const TypedefDecl *Typedef) {
  std::println(stderr, "{} {} '{}'", "typedef", Typedef->getName(),
               Typedef->getUnderlying().getAsString());
}

void ASTDumper::visit(const Stmt *S) {
  if (!S) {
    printNull();
    return;
  }

  switch (S->getKind()) {
  case Stmt::SK_DeclStmt:
    visit(cast<DeclStmt>(S));
    break;
  case Stmt::SK_ReturnStmt:
    visit(cast<ReturnStmt>(S));
    break;
  case Stmt::SK_CompoundStmt:
    visit(cast<CompoundStmt>(S));
    break;
  case Stmt::SK_NullStmt:
    visit(cast<NullStmt>(S));
    break;
  case Stmt::SK_IfStmt:
    visit(cast<IfStmt>(S));
    break;
  case Stmt::SK_ForStmt:
    visit(cast<ForStmt>(S));
    break;
  case Stmt::SK_WhileStmt:
    visit(cast<WhileStmt>(S));
    break;
  case Stmt::SK_UnaryOperator:
    visit(cast<UnaryOperator>(S));
    break;
  case Stmt::SK_BinaryOperator:
    visit(cast<BinaryOperator>(S));
    break;
  case Stmt::SK_IntegerLiteral:
    visit(cast<IntegerLiteral>(S));
    break;
  case Stmt::SK_ParenExpr:
    visit(cast<ParenExpr>(S));
    break;
  case Stmt::SK_DeclRefExpr:
    visit(cast<DeclRefExpr>(S));
    break;
  case Stmt::SK_CallExpr:
    visit(cast<CallExpr>(S));
    break;
  case Stmt::SK_ArraySubscriptExpr:
    visit(cast<ArraySubscriptExpr>(S));
    break;
  case Stmt::SK_UnaryExprOrTypeTraitExpr:
    visit(cast<UnaryExprOrTypeTraitExpr>(S));
    break;
  case Stmt::SK_CharacterLiteral:
    visit(cast<CharacterLiteral>(S));
    break;
  case Stmt::SK_StringLiteral:
    visit(cast<StringLiteral>(S));
    break;
  case Stmt::SK_MemberExpr:
    visit(cast<MemberExpr>(S));
    break;
  case Stmt::SK_CastExpr:
    visit(cast<CastExpr>(S));
    break;
  case Stmt::SK_StmtExpr:
    visit(cast<StmtExpr>(S));
    break;
  default:
    RCC_UNREACHABLE("unknown stmt kind");
  }
}

void ASTDumper::visit(const DeclStmt *DS) {
  printName("DeclStmt");
  bool IsSingle = DS->getNumDecls() == 1;
  for (unsigned I = 0; I < DS->getNumDecls(); ++I) {
    ScopedIndent SI(*this, IsSingle);
    visit(DS->getDecl(I));
  }
}

void ASTDumper::visit(const ReturnStmt *Ret) {
  printName("ReturnStmt");
  const auto *RetVal = Ret->getRetValue();
  if (RetVal) {
    ScopedIndent SI(*this, true);
    visit(RetVal);
  }
}

void ASTDumper::visit(const CompoundStmt *CS) {
  printName("CompoundStmt");
  bool IsSingle = CS->getBody().size() == 1;
  for (const auto *S : CS->getBody()) {
    ScopedIndent SI(*this, IsSingle);
    visit(S);
  }
}

void ASTDumper::visit(const NullStmt *) { printName("NullStmt"); }

void ASTDumper::visit(const IfStmt *If) {
  printName("IfStmt");
  {
    ScopedIndent SI(*this, false);
    visit(If->getCond());
  }
  {
    ScopedIndent SI(*this, false);
    visit(If->getThen());
  }

  if (const Stmt *Else = If->getElse()) {
    ScopedIndent SI(*this, false);
    visit(Else);
  }
}

void ASTDumper::visit(const ForStmt *For) {
  printName("ForStmt");
  {
    ScopedIndent SI(*this, false);
    visit(For->getInit());
  }
  {
    ScopedIndent SI(*this, false);
    visit(For->getCond());
  }
  {
    ScopedIndent SI(*this, false);
    visit(For->getInc());
  }
  {
    ScopedIndent SI(*this, false);
    visit(For->getBody());
  }
}

void ASTDumper::visit(const WhileStmt *While) {
  printName("WhileStmt");
  {
    ScopedIndent SI(*this, false);
    visit(While->getCond());
  }
  {
    ScopedIndent SI(*this, false);
    visit(While->getBody());
  }
}

void ASTDumper::visit(const UnaryOperator *UO) {
  std::println(stderr, "UnaryOperator prefix '{}'", UO->getOpcodeStr());
  ScopedIndent SI(*this, true);
  visit(UO->getSubExpr());
}

void ASTDumper::visit(const BinaryOperator *BO) {
  std::println(stderr, "BinaryOperator '{}'", BO->getOpcodeStr());
  {
    ScopedIndent SI(*this, false);
    visit(BO->getLHS());
  }
  {
    ScopedIndent SI(*this, false);
    visit(BO->getRHS());
  }
}

void ASTDumper::visit(const IntegerLiteral *IL) {
  std::println(stderr, "IntegerLiteral {}", IL->getVal());
}

void ASTDumper::visit(const ParenExpr *Paren) {
  printName("ParenExpr");
  ScopedIndent SI(*this, true);
  visit(Paren->getSubExpr());
}

void ASTDumper::visit(const DeclRefExpr *Ref) {
  const auto *ND = dynCast<NamedDecl>(Ref->getDecl());
  const std::string &Name = ND ? ND->getName() : "";
  std::println(stderr, "DeclRefExpr '{}' '{}'", Ref->getType().getAsString(),
               Name);
}

void ASTDumper::visit(const CallExpr *Call) {
  printName("CallExpr");
  {
    ScopedIndent SI(*this, false);
    visit(Call->getCallee());
  }
  for (const auto *E : Call->getArgs()) {
    ScopedIndent SI(*this, false);
    visit(E);
  }
}

void ASTDumper::visit(const ArraySubscriptExpr *ASE) {
  printName("ArraySubscriptExpr");
  {
    ScopedIndent SI(*this, false);
    visit(ASE->getLHS());
  }
  {
    ScopedIndent SI(*this, true);
    visit(ASE->getRHS());
  }
}

void ASTDumper::visit(const CharacterLiteral *CL) {
  std::println(stderr, "CharacterLiteral '{}' '{}'",
               CL->getType().getAsString(), static_cast<char>(CL->getValue()));
}

void ASTDumper::visit(const StringLiteral *SL) {
  std::println(stderr, "StringLiteral '{}' {}", SL->getType().getAsString(),
               SL->getString());
}

void ASTDumper::visit(const UnaryExprOrTypeTraitExpr *UE) {
  if (UE->isArgumentType()) {
    std::println(stderr, "UnaryExprOrTypeTraitExpr sizeof '{}'",
                 UE->getArgumentType().getAsString());
    return;
  }
  printName("UnaryExprOrTypeTraitExpr sizeof");
  ScopedIndent SI(*this, true);
  visit(UE->getArgumentExpr());
}

void ASTDumper::visit(const MemberExpr *ME) {
  std::println(stderr, "MemberExpr '{}' {} {}", ME->getType().getAsString(),
               ME->isArrow() ? "->" : ".",
               ME->getBase()->getType().getAsString());
  ScopedIndent SI(*this, true);
  visit(ME->getBase());
}

void ASTDumper::visit(const CastExpr *Cast) {
  std::println(stderr, "{}CastExpr '{}' <{}>",
               Cast->isImplicit() ? "Implicit" : "Explicit",
               Cast->getType().getAsString(), Cast->getCastKindStr());
  ScopedIndent SI(*this, true);
  visit(Cast->getSubExpr());
}

void ASTDumper::visit(const StmtExpr *SE) {
  printName("StmtExpr");
  ScopedIndent SI(*this, true);
  visit(SE->getSubStmt());
}

void ASTDumper::printName(const char *Name) const {
  std::println(stderr, "{}", Name);
}

void ASTDumper::printName(const ValueDecl *D) const {
  std::println(stderr, "{} {} '{}'", D->getKindStr(), D->getName(),
               D->getType().getAsString());
}

void ASTDumper::printName(const TagDecl *D) const {
  std::println(stderr, "{} {} '{}'", D->getKindStr(), D->getName(),
               D->getType().getAsString());
}

void ASTDumper::printNull() const { std::println(stderr, "<<<NULL>>>"); }

void ASTDumper::indent(bool IsSingle) {
  if (IsSingle) {
    Indent += 2;
    return;
  }

  PipeIndents.push_back(Indent);
  Indent += 1;
}

void ASTDumper::unindent(bool IsSingle) {
  if (IsSingle) {
    Indent -= 2;
    return;
  }

  PipeIndents.pop_back();
  Indent -= 1;
}

void ASTDumper::printIndent() const {
  unsigned Last = 0;
  for (unsigned Pipe : PipeIndents) {
    std::print(stderr, "{:>{}}|", "", Pipe - Last);
    Last = Pipe;
  }
  std::print(stderr, "{:>{}}", "", Indent - Last);
}

} // namespace rcc