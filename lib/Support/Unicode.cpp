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

static bool inRange(const std::uint32_t *Range, std::uint32_t C) {
  for (int I = 0; Range[I] != static_cast<std::uint32_t>(-1); I += 2)
    if (Range[I] <= C && C <= Range[I + 1])
      return true;
  return false;
}

bool isIdentStart(std::uint32_t C) {
  // C11 Annex D allowed identifier characters (excluding ASCII digits),
  // plus GNU '$' as an identifier character.
  // clang-format off
  static const std::uint32_t Range[] = {
      '_',     '_',
      'a',     'z',
      'A',     'Z',
      '$',     '$',
      0x00A8,  0x00A8,
      0x00AA,  0x00AA,
      0x00AD,  0x00AD,
      0x00AF,  0x00AF,
      0x00B2,  0x00B5,
      0x00B7,  0x00BA,
      0x00BC,  0x00BE,
      0x00C0,  0x00D6,
      0x00D8,  0x00F6,
      0x00F8,  0x00FF,
      0x0100,  0x02FF,
      0x0370,  0x167F,
      0x1681,  0x180D,
      0x180F,  0x1DBF,
      0x1E00,  0x1FFF,
      0x200B,  0x200D,
      0x202A,  0x202E,
      0x203F,  0x2040,
      0x2054,  0x2054,
      0x2060,  0x206F,
      0x2070,  0x20CF,
      0x2100,  0x218F,
      0x2460,  0x24FF,
      0x2776,  0x2793,
      0x2C00,  0x2DFF,
      0x2E80,  0x2FFF,
      0x3004,  0x3007,
      0x3021,  0x302F,
      0x3031,  0x303F,
      0x3040,  0xD7FF,
      0xF900,  0xFD3D,
      0xFD40,  0xFDCF,
      0xFDF0,  0xFE1F,
      0xFE30,  0xFE44,
      0xFE47,  0xFFFD,
      0x10000, 0x1FFFD,
      0x20000, 0x2FFFD,
      0x30000, 0x3FFFD,
      0x40000, 0x4FFFD,
      0x50000, 0x5FFFD,
      0x60000, 0x6FFFD,
      0x70000, 0x7FFFD,
      0x80000, 0x8FFFD,
      0x90000, 0x9FFFD,
      0xA0000, 0xAFFFD,
      0xB0000, 0xBFFFD,
      0xC0000, 0xCFFFD,
      0xD0000, 0xDFFFD,
      0xE0000, 0xEFFFD,
      static_cast<std::uint32_t>(-1),
  };
  // clang-format on

  return inRange(Range, C);
}

bool isIdentContinue(std::uint32_t C) {
  // clang-format off
  static const std::uint32_t Range[] = {
      '0',     '9',
      0x0300,  0x036F,
      0x1DC0,  0x1DFF,
      0x20D0,  0x20FF,
      0xFE20,  0xFE2F,
      static_cast<std::uint32_t>(-1),
  };
  // clang-format on

  return isIdentStart(C) || inRange(Range, C);
}

} // namespace rcc
