#ifndef RCC_AST_ASTDUMPER
#define RCC_AST_ASTDUMPER

#include "AST/Stmt.h"
#include <vector>

namespace rcc {

class Decl;
class TranslationUnitDecl;
class ValueDecl;
class TypeDecl;
class VarDecl;
class EnumConstantDecl;
class EnumDecl;
class FunctionDecl;

class Stmt;
class DeclStmt;
class ReturnStmt;
class CompoundStmt;
class NullStmt;
class IfStmt;
class ForStmt;
class WhileStmt;
class SwitchStmt;
class CaseStmt;
class DefaultStmt;
class BreakStmt;
class ContinueStmt;
class GotoStmt;
class LabelStmt;
class UnaryOperator;
class BinaryOperator;
class IntegerLiteral;
class ParenExpr;
class DeclRefExpr;
class CallExpr;

class ASTDumper {
public:
  ASTDumper() = default;

  void visit(const Decl *D);
  void visit(const TranslationUnitDecl *TU);
  void visit(const FunctionDecl *Func);
  void visit(const VarDecl *Var);
  void visit(const EnumConstantDecl *ECD);
  void visit(const FieldDecl *Field);
  void visit(const RecordDecl *Record);
  void visit(const EnumDecl *Enum);
  void visit(const TypedefDecl *Typedef);

public:
  void visit(const Stmt *S);
  void visit(const DeclStmt *DS);
  void visit(const ReturnStmt *Ret);
  void visit(const CompoundStmt *CS);
  void visit(const NullStmt *NS);
  void visit(const IfStmt *If);
  void visit(const ForStmt *For);
  void visit(const WhileStmt *While);
  void visit(const SwitchStmt *Switch);
  void visit(const CaseStmt *Case);
  void visit(const DefaultStmt *Default);
  void visit(const BreakStmt *Break);
  void visit(const ContinueStmt *Continue);
  void visit(const GotoStmt *Goto);
  void visit(const LabelStmt *Label);
  void visit(const UnaryOperator *UO);
  void visit(const BinaryOperator *BO);
  void visit(const IntegerLiteral *IL);
  void visit(const ParenExpr *Paren);
  void visit(const DeclRefExpr *Ref);
  void visit(const CallExpr *Call);
  void visit(const ArraySubscriptExpr *ASE);
  void visit(const CharacterLiteral *CL);
  void visit(const StringLiteral *SL);
  void visit(const UnaryExprOrTypeTraitExpr *UE);
  void visit(const MemberExpr *ME);
  void visit(const CastExpr *Cast);
  void visit(const StmtExpr *SE);

private:
  friend class ScopedIndent;

  void printName(const char *Name) const;
  void printName(const ValueDecl *D) const;
  void printName(const TagDecl *D) const;
  void printNull() const;

  void indent(bool IsSingle);
  void unindent(bool IsSingle);
  void printIndent() const;

private:
  unsigned Indent = 0;
  std::vector<unsigned> PipeIndents;
};

} // namespace rcc

#endif