#include "AST/ASTContext.h"

namespace rcc {

void ASTContext::initBuiltinTypes() {
  initBuiltinType(IntTy, BuiltinType::BK_Int);
}

void ASTContext::initBuiltinType(CanQualType &R, BuiltinType::Kind Kind) {
  auto *Ty = new (*this, alignof(BuiltinType)) BuiltinType(Kind);
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

QualType ASTContext::getFunctionType() {
  auto *Ty = new (*this, alignof(FunctionType)) FunctionType();
  return QualType(Ty);
}

} // namespace rcc