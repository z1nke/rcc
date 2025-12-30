#include "AST/ASTContext.h"

namespace rcc {

void ASTContext::initBuiltinTypes() {
  initBuiltinType(IntTy, BuiltinType::BK_Int, 8);
}

void ASTContext::initBuiltinType(CanQualType &R, BuiltinType::Kind Kind,
                                 std::size_t Size) {
  auto *Ty = new (*this, alignof(BuiltinType)) BuiltinType(Kind, Size);
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

} // namespace rcc