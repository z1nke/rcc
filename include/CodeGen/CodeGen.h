#ifndef RCC_CODEGEN_CODEGEN_H
#define RCC_CODEGEN_CODEGEN_H

#include "AST/Stmt.h"

#include <cstdint>
#include <print>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rcc {

class Decl;
class TranslationUnitDecl;
class FunctionDecl;
class VarDecl;
class Type;
class Diagnostic;

class CodeGen {
public:
  CodeGen(Diagnostic &Diag, FILE *Fp);

  void setEmitCommon(bool V) { EmitCommon = V; }

  void codegen(const TranslationUnitDecl *TU, const char *Input);

private:
  void emitData(const TranslationUnitDecl *TU);
  void emitText(const TranslationUnitDecl *TU);

  static bool mustBeEmitted(const FunctionDecl *FD);
  void addDeferredDeclToEmit(const FunctionDecl *FD);
  void noteDeferredUse(const FunctionDecl *FD);
  void emitDeferred();

private:
  void genFunction(const FunctionDecl *FD);

private:
  void genStmt(const Stmt *S);
  void genDeclStmt(const DeclStmt *DS);
  void genIfStmt(const IfStmt *If);
  void genForStmt(const ForStmt *For);
  void genWhileStmt(const WhileStmt *While);
  void genDoWhileStmt(const DoWhileStmt *DoWhile);
  void genSwitchStmt(const SwitchStmt *Switch);
  void genCaseStmt(const CaseStmt *Case);
  void genDefaultStmt(const DefaultStmt *Default);
  void genBreakStmt(const BreakStmt *Break);
  void genContinueStmt(const ContinueStmt *Continue);
  void genGotoStmt(const GotoStmt *Goto);
  void genLabelStmt(const LabelStmt *Label);
  void genAsmStmt(const AsmStmt *AS);
  void genExpr(const Expr *E);
  void genStringLiteral(const StringLiteral *SL);
  void genDeclRefExpr(const DeclRefExpr *Ref);
  void genBinaryOperator(const BinaryOperator *BO);
  void genConditionalOperator(const ConditionalOperator *CO);
  void genBinaryConditionalOperator(const BinaryConditionalOperator *BCO);

  /// Emits a0 = (lhs op rhs) given a0 = lhs and a1 = rhs.
  void emitBinaryArithmeticResult(BinaryOperator::Opcode Op, QualType LType,
                                  QualType RType, const char *Suffix);
  void genUnaryOperator(const UnaryOperator *UO);
  void genCallExpr(const CallExpr *CE);
  void emitBuiltinAlloca();
  void genArraySubscriptExpr(const ArraySubscriptExpr *ASE);
  void genUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *UE);
  void genCastExpr(const CastExpr *Cast);
  void genCompoundLiteralExpr(const CompoundLiteralExpr *CLE);
  void emitLocalVarInit(const VarDecl *Var);

  // VLA
  void emitVariablyModifiedType(QualType Ty);
  void emitVLAByteSize(const VariableArrayType *VAT);
  int getVLASizeSlot(const Expr *SizeExpr);
  void scaleIndexByTypeSize(QualType ElemTy, const char *IndexReg);

  void genInitListExpr(const VarDecl *Var, const InitListExpr *List,
                       QualType AggTy, std::size_t BaseOffset);
  void genInitListExprFromFlat(const VarDecl *Var, const InitListExpr *List,
                               QualType AggTy, std::size_t BaseOffset,
                               std::size_t &Idx);
  void genInitListElement(const VarDecl *Var, const Expr *ElemInit,
                          QualType ElemTy, std::size_t Offset,
                          const FieldDecl *Field = nullptr);
  void genStringLiteralInit(const VarDecl *Var, const StringLiteral *SL,
                            QualType ArrTy, std::size_t BaseOffset);
  void genZeroInit(const VarDecl *Var, QualType Ty, std::size_t BaseOffset);
  void emitGlobalVarInit(const VarDecl *Var, const Expr *Init);
  void emitGlobalInit(const Expr *Init, QualType Ty, std::size_t BaseOffset);
  void emitGlobalInitFromFlat(const InitListExpr *List, QualType Ty,
                              std::size_t BaseOffset, std::size_t &Idx);
  void emitGlobalStringLiteralInit(const StringLiteral *SL, QualType ArrTy,
                                   std::size_t BaseOffset);
  void emitGlobalZeroInit(QualType Ty, std::size_t BaseOffset);
  void emitScalarData(std::size_t Offset, std::size_t Size, std::int64_t Val);

  void writeGlobalInitToBuf(std::vector<std::uint8_t> &Buf, std::size_t Offset,
                            const Expr *Init, QualType Ty);
  void writeGlobalInitToBufFromFlat(std::vector<std::uint8_t> &Buf,
                                    std::size_t Offset, const InitListExpr *List,
                                    QualType Ty, std::size_t &Idx);
  void emitDataBuf(const std::vector<std::uint8_t> &Buf);

  void genAddr(const Expr *E);
  void genAddr(const Decl *D);
  void genAddr(const ArraySubscriptExpr *ASE);
  void genAddr(const StringLiteral *SL);
  void genAddr(const MemberExpr *ME);

private:
  void push();
  void pop(const char *Reg);
  void pushF();
  void popF(const char *Reg);
  /// Soft-float long double live values on fs0–fs11 pairs.
  void pushLD();
  void popLD(int Reg);
  void load(const Type *Ty);
  void store(const Type *Ty);

  /// Extract a bit-field from a0 into a0 (after a full-width load).
  void loadBitField(const FieldDecl *Field);
  /// Merge a0 into the bit-field at the address on the stack (0(sp)).
  void storeBitField(const FieldDecl *Field);

  /// If \p Ty is floating, set a0 to 1 when fa0 != 0.0, else 0.
  void emitIsNotZero(const Type *Ty);

  void storeGenReg(int Reg, int Offset, int Size);
  void storeFloatReg(int Reg, int Offset, int Size);
  void loadGenRegFromT1(int Reg, int Offset, int Size);
  void loadFloatRegFromT1(int Reg, int Offset, int Size);
  void pushStructArg(const Type *Ty, bool OnStack);
  void popStructArgToRegs(const Type *Ty, int &GP, int &FP, bool OnStack);
  void storeStructParam(const Type *Ty, int Offset, int &GP, int &FP,
                        bool HalfByStack = false);
  void copyRetBuffer(const VarDecl *Buf);
  void copyStructReg();
  void copyStructMem();
  int createBigStructCallSpace(const CallExpr *CE);

  /// Returns the RISC-V load/store width suffix ("b"/"h"/"w"/"d").
  /// When \p IsUnsigned is true, returns the zero-extending load suffix
  /// ("bu"/"hu"/"wu"; "d" is unchanged).
  const char *getWidthSuffix(std::size_t Size, bool IsUnsigned = false) const;

  int getCount() const;
  int simpleLog2(int Num);
  const std::string &getStringLabel(const StringLiteral *SL);
  const std::string &getVarSymbol(const VarDecl *Var);

  void genIntCast(const Type *From, const Type *To);
  void genScalarCast(const Type *From, const Type *To);

private:
  template <typename... ARGS>
  void emit(std::format_string<ARGS...> Fmt, ARGS &&...Args) const {
    std::println(Fp, Fmt, std::forward<ARGS &&>(Args)...);
  }

private:
  Diagnostic &Diag;
  const FunctionDecl *CurrFunc = nullptr;
  int Depth = 0;
  int BigStructDepth = 0;
  /// Long-double soft-float stack pointer into fs0–fs11 (steps of 2).
  int LDSP = 0;
  std::vector<int> BreakCounts;
  std::vector<int> ContinueCounts;
  std::vector<int> SwitchCounts;
  std::vector<const StringLiteral *> StringLiterals;
  std::unordered_map<const StringLiteral *, std::string> SLCache;
  std::unordered_map<const VarDecl *, std::string> StaticLocalNames;
  std::unordered_map<std::string, const FunctionDecl *> DeferredDecls;
  std::vector<const FunctionDecl *> DeferredDeclsToEmit;
  std::unordered_set<const FunctionDecl *> EmittedDecls;
  /// Maps VLA SizeExpr → fp-relative offset of the cached element count.
  std::unordered_map<const Expr *, int> VLASizeMap;
  std::size_t CurrStackSize = 0;
  int VLASizeExtra = 0;
  unsigned AnonGVarId = 0;
  bool EmitCommon = true;
  FILE *Fp;
};

} // namespace rcc

#endif