#include "AST/ASTContext.h"
#include "AST/Decl.h"

namespace rcc {

void ASTContext::initBuiltinTypes() {
  initBuiltinType(VoidTy, BuiltinType::BK_Void, 1, 1);
  initBuiltinType(CharTy, BuiltinType::BK_Char, 1, 1);
  initBuiltinType(ShortTy, BuiltinType::BK_Short, 2, 2);
  initBuiltinType(IntTy, BuiltinType::BK_Int, 4, 4);
  initBuiltinType(LongTy, BuiltinType::BK_Long, 8, 8);
}

void ASTContext::initBuiltinType(CanQualType &R, BuiltinType::Kind Kind,
                                 std::size_t Size, std::size_t Align) {
  auto *Ty = new (*this, alignof(BuiltinType)) BuiltinType(Kind, Size, Align);
  R = CanQualType(Ty);
  Types.push_back(Ty);
}

QualType ASTContext::getPointerType(QualType PointeeType) {
  assert(!PointeeType.isNull());
  void *Opaque = PointeeType.getOpaquePtr();
  auto Iter = PointerTypes.find(Opaque);
  if (Iter != PointerTypes.end())
    return Iter->second;

  auto *Ty = new (*this, alignof(PointerType)) PointerType(PointeeType);
  PointerTypes[Opaque] = Ty;
  return QualType(Ty);
}

QualType ASTContext::getFunctionType(QualType RetType,
                                     std::vector<QualType> ParamTypes) {
  auto *Ty = new (*this, alignof(FunctionType))
      FunctionType(RetType, std::move(ParamTypes));
  return QualType(Ty);
}

QualType ASTContext::getConstantArrayType(QualType ElementType,
                                          std::size_t Len) {
  auto *Ty = new (*this, alignof(ConstantArrayType))
      ConstantArrayType(ElementType, Len);
  return QualType(Ty);
}

QualType ASTContext::getRecordType(RecordDecl *RD, std::size_t Size,
                                   std::size_t Align) {
  const auto *Canonical = RD->getCanonicalDecl();
  RecordType *&Ty = RecordTypes[Canonical];
  if (Ty)
    return QualType(Ty);

  Ty = new (*this, alignof(RecordType)) RecordType(RD, Size, Align);
  return QualType(Ty);
}

} // namespace rcc