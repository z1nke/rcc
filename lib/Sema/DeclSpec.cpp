#include "Sema/DeclSpec.h"

namespace rcc {

DeclaratorChunk DeclaratorChunk::createPointer() {
  return DeclaratorChunk(DCK_Pointer);
}

DeclaratorChunk DeclaratorChunk::createFunction() {
  return DeclaratorChunk(DCK_Function);
}

void DeclSpec::setTypeSpecType(TypeSpecType TST, SourceLocation TSTLoc) {
  this->TST = TST;
  this->TSTLoc = TSTLoc;
}

} // namespace rcc