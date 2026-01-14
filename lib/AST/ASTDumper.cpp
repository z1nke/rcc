#include "AST/ASTDumper.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/Unreachable.h"

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
  default:
    RCC_UNREACHABLE("unknown decl kind");
  }
}

class ScopeIndent {
public:
  ScopeIndent(ASTDumper &Dumper, bool IsSingle)
      : Dumper(Dumper), IsSingle(IsSingle) {
    Dumper.printIndent();
    std::print(stderr, "{}", IsSingle ? SinglePrefix : MultiPrefix);
    Dumper.indent(IsSingle);
  }
  ~ScopeIndent() { Dumper.unindent(IsSingle); }

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
    ScopeIndent SI(*this, IsSingle);
    visit(D);
  }
}

void ASTDumper::visit(const FunctionDecl *Func) {
  printName(Func);
  for (const auto *Param : Func->getParams()) {
    ScopeIndent SI(*this, false);
    visit(Param);
  }

  if (Stmt *Body = Func->getBody()) {
    ScopeIndent SI(*this, false);
    visit(Body);
  }
}

void ASTDumper::visit(const VarDecl *Var) {
  printName(Var);
  if (const Expr *Init = Var->getInit()) {
    ScopeIndent SI(*this, true);
    visit(Init);
  }
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
  default:
    RCC_UNREACHABLE("unknown stmt kind");
  }
}

void ASTDumper::visit(const DeclStmt *DS) {
  printName("DeclStmt");
  bool IsSingle = DS->getNumDecls() == 1;
  for (unsigned I = 0; I < DS->getNumDecls(); ++I) {
    ScopeIndent SI(*this, IsSingle);
    visit(DS->getDecl(I));
  }
}

void ASTDumper::visit(const ReturnStmt *Ret) {
  printName("ReturnStmt");
  const auto *RetVal = Ret->getRetValue();
  if (RetVal) {
    ScopeIndent SI(*this, true);
    visit(RetVal);
  }
}

void ASTDumper::visit(const CompoundStmt *CS) {
  printName("CompoundStmt");
  bool IsSingle = CS->getBody().size() == 1;
  for (const auto *S : CS->getBody()) {
    ScopeIndent SI(*this, IsSingle);
    visit(S);
  }
}

void ASTDumper::visit(const NullStmt *) { printName("NullStmt"); }

void ASTDumper::visit(const IfStmt *If) {
  printName("IfStmt");
  {
    ScopeIndent SI(*this, false);
    visit(If->getCond());
  }
  {
    ScopeIndent SI(*this, false);
    visit(If->getThen());
  }

  if (const Stmt *Else = If->getElse()) {
    ScopeIndent SI(*this, false);
    visit(Else);
  }
}

void ASTDumper::visit(const ForStmt *For) {
  printName("ForStmt");
  {
    ScopeIndent SI(*this, false);
    visit(For->getInit());
  }
  {
    ScopeIndent SI(*this, false);
    visit(For->getCond());
  }
  {
    ScopeIndent SI(*this, false);
    visit(For->getInc());
  }
  {
    ScopeIndent SI(*this, false);
    visit(For->getBody());
  }
}

void ASTDumper::visit(const WhileStmt *While) {
  printName("WhileStmt");
  {
    ScopeIndent SI(*this, false);
    visit(While->getCond());
  }
  {
    ScopeIndent SI(*this, false);
    visit(While->getBody());
  }
}

void ASTDumper::visit(const UnaryOperator *UO) {
  std::println(stderr, "UnaryOperator prefix '{}'", UO->getOpcodeStr());
  ScopeIndent SI(*this, true);
  visit(UO->getSubExpr());
}

void ASTDumper::visit(const BinaryOperator *BO) {
  std::println(stderr, "BinaryOperator '{}'", BO->getOpcodeStr());
  {
    ScopeIndent SI(*this, false);
    visit(BO->getLHS());
  }
  {
    ScopeIndent SI(*this, false);
    visit(BO->getRHS());
  }
}

void ASTDumper::visit(const IntegerLiteral *IL) {
  std::println(stderr, "IntegerLiteral {}", IL->getVal());
}

void ASTDumper::visit(const ParenExpr *Paren) {
  printName("ParenExpr");
  ScopeIndent SI(*this, true);
  visit(Paren->getSubExpr());
}

void ASTDumper::visit(const DeclRefExpr *Ref) {
  printName("DeclRefExpr");
  ScopeIndent SI(*this, true);
  visit(Ref->getDecl());
}

void ASTDumper::visit(const CallExpr *Call) {
  printName("CallExpr");
  {
    ScopeIndent SI(*this, false);
    visit(Call->getCallee());
  }
  for (const auto *E : Call->getArgs()) {
    ScopeIndent SI(*this, false);
    visit(E);
  }
}

void ASTDumper::visit(const ArraySubscriptExpr *ASE) {
  printName("ArraySubscriptExpr");
  {
    ScopeIndent SI(*this, false);
    visit(ASE->getLHS());
  }
  {
    ScopeIndent SI(*this, true);
    visit(ASE->getRHS());
  }
}

void ASTDumper::printName(const char *Name) const {
  std::println(stderr, "{}", Name);
}

void ASTDumper::printName(const ValueDecl *D) const {
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