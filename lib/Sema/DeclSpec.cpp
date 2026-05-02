#include "Sema/DeclSpec.h"
#include "Basic/Diagnostic.h"
#include "Support/Unreachable.h"

namespace rcc {

DeclaratorChunk DeclaratorChunk::createPointer() {
  return DeclaratorChunk(DCK_Pointer);
}

DeclaratorChunk DeclaratorChunk::createFunction(bool IsVariadic) {
  DeclaratorChunk Chunk(DCK_Function);
  Chunk.Fun.IsVariadic = IsVariadic;
  return Chunk;
}

DeclaratorChunk DeclaratorChunk::createArray(Expr *LenExpr) {
  DeclaratorChunk Chunk(DCK_Array);
  Chunk.Arr.LenExpr = LenExpr;
  return Chunk;
}

bool DeclSpec::hasTypeSpecifier() const {
  return TST != TST_Unspecified || TSW != TSW_Unspecified ||
         TSS != TSS_Unspecified;
}

void DeclSpec::setStorageClassSpec(StorageClassSpec S, SourceLocation Loc) {
  if (SCS != SCS_Unspecified)
    reportBadSpec(Loc, S, SCS);

  SCS = S;
  SCSLoc = Loc;
}

void DeclSpec::setTypeSpecType(TypeSpecType T, SourceLocation Loc) {
  if (TST != TST_Unspecified)
    reportBadSpec(TSLoc, T, TST);

  TST = T;
  TSLoc = Loc;
}

void DeclSpec::setTypeSpecWidth(TypeSpecWidth W, SourceLocation Loc) {
  if (TSLoc.isInvalid())
    TSLoc = Loc;

  if (TSW == TSW_Unspecified) {
    TSW = W;
    return;
  }

  if (W == TSW_Long && TSW == TSW_Long) {
    TSW = TSW_LongLong;
    return;
  }

  reportBadSpec(TSLoc, W, TSW);
}

void DeclSpec::setTypeSpecSign(TypeSpecSign S, SourceLocation Loc) {
  if (TSLoc.isInvalid())
    TSLoc = Loc;

  if (TSS != TSS_Unspecified)
    reportBadSpec(Loc, S, TSS);

  TSS = S;
}

template <typename T>
void DeclSpec::reportBadSpec(SourceLocation Loc, T New, T Prev) {
  if (New != Prev) {
    Diag.fatalAt(Loc, "cannot combine with previous '{}' declaration specifier",
                 getSpecifierName(Prev));
  }

  Diag.fatalAt(Loc, "duplicate '{}' declaration specifier",
               getSpecifierName(Prev));
}

const char *DeclSpec::getSpecifierName(StorageClassSpec S) {
  switch (S) {
  case SCS_Typedef:
    return "typedef";
  case SCS_Extern:
    return "extern";
  case SCS_Static:
    return "static";
  default:
    RCC_UNREACHABLE("Unknown storage class specifier");
  }
}

const char *DeclSpec::getSpecifierName(TypeSpecType T) {
  switch (T) {
  case TST_Void:
    return "void";
  case TST_Char:
    return "char";
  case TST_UnderlineBool:
    return "_Bool";
  case TST_Int:
    return "int";
  case TST_Struct:
    return "struct";
  case TST_Union:
    return "union";
  case TST_Typename:
    return "typename";
  default:
    RCC_UNREACHABLE("Unknown type specifier type");
  }
}
const char *DeclSpec::getSpecifierName(TypeSpecWidth T) {
  switch (T) {
  case TSW_Short:
    return "short";
  case TSW_Long:
    return "long";
  default:
    RCC_UNREACHABLE("Unknown type specifier width");
  }
}

const char *DeclSpec::getSpecifierName(TypeSpecSign T) {
  switch (T) {
  case TSS_Signed:
    return "signed";
  case TSS_Unsigned:
    return "unsigned";
  default:
    RCC_UNREACHABLE("Unknown type specifier sign");
  }
}

} // namespace rcc