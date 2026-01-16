#include "Sema/DeclSpec.h"
#include <algorithm>

namespace rcc {

DeclaratorChunk DeclaratorChunk::createPointer() {
  return DeclaratorChunk(DCK_Pointer);
}

DeclaratorChunk DeclaratorChunk::createFunction() {
  return DeclaratorChunk(DCK_Function);
}

DeclaratorChunk DeclaratorChunk::createArray(Expr *LenExpr) {
  DeclaratorChunk Chunk(DCK_Array);
  Chunk.Arr.LenExpr = LenExpr;
  return Chunk;
}

void DeclSpec::setTypeSpecType(TypeSpecType TST, SourceLocation TSTLoc) {
  this->TST = TST;
  this->TSTLoc = TSTLoc;
}

} // namespace rcc