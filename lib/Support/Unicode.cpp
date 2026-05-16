#include "Support/Unicode.h"
#include "Basic/Diagnostic.h"

namespace rcc {

int encodeUTF8(char *Buf, std::uint32_t C) {
  if (C <= 0x7F) {
    Buf[0] = static_cast<char>(C);
    return 1;
  }

  if (C <= 0x7FF) {
    Buf[0] = static_cast<char>(0xC0 | (C >> 6));
    Buf[1] = static_cast<char>(0x80 | (C & 0x3F));
    return 2;
  }

  if (C <= 0xFFFF) {
    Buf[0] = static_cast<char>(0xE0 | (C >> 12));
    Buf[1] = static_cast<char>(0x80 | ((C >> 6) & 0x3F));
    Buf[2] = static_cast<char>(0x80 | (C & 0x3F));
    return 3;
  }

  Buf[0] = static_cast<char>(0xF0 | (C >> 18));
  Buf[1] = static_cast<char>(0x80 | ((C >> 12) & 0x3F));
  Buf[2] = static_cast<char>(0x80 | ((C >> 6) & 0x3F));
  Buf[3] = static_cast<char>(0x80 | (C & 0x3F));
  return 4;
}

std::uint32_t decodeUTF8(const char **NewPos, const char *P, Diagnostic &Diag) {
  if (static_cast<unsigned char>(*P) < 128) {
    *NewPos = P + 1;
    return static_cast<unsigned char>(*P);
  }

  const char *Start = P;
  int Len;
  std::uint32_t C;

  if (static_cast<unsigned char>(*P) >= 0xF0) {
    Len = 4;
    C = *P & 0x07;
  } else if (static_cast<unsigned char>(*P) >= 0xE0) {
    Len = 3;
    C = *P & 0x0F;
  } else if (static_cast<unsigned char>(*P) >= 0xC0) {
    Len = 2;
    C = *P & 0x1F;
  } else {
    Diag.fatalAt(Start, "invalid UTF-8 sequence");
  }

  for (int I = 1; I < Len; ++I) {
    if ((static_cast<unsigned char>(P[I]) >> 6) != 0x2)
      Diag.fatalAt(Start, "invalid UTF-8 sequence");
    C = (C << 6) | (P[I] & 0x3F);
  }

  *NewPos = P + Len;
  return C;
}

} // namespace rcc
