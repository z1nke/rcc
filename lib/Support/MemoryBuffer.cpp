#include "Support/MemoryBuffer.h"
#include "Support/Error.h"
#include "Support/Unicode.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace rcc {

namespace {

static int fromHex(char C) {
  if ('0' <= C && C <= '9')
    return C - '0';
  if ('a' <= C && C <= 'f')
    return C - 'a' + 10;
  if ('A' <= C && C <= 'F')
    return C - 'A' + 10;
  return -1;
}

static std::uint32_t readUniversalChar(const char *P, int Len) {
  std::uint32_t C = 0;
  for (int I = 0; I < Len; ++I) {
    int Digit = fromHex(P[I]);
    if (Digit < 0)
      return 0;
    C = (C << 4) | static_cast<std::uint32_t>(Digit);
  }
  return C;
}

static bool startsWith(const char *P, const char *Prefix) {
  for (; *Prefix; ++P, ++Prefix) {
    if (*P != *Prefix)
      return false;
  }
  return true;
}

} // namespace

std::unique_ptr<MemoryBuffer>
MemoryBuffer::fromFile(const std::string &Filename) {
  std::ifstream File(Filename, std::ios::in | std::ios::binary);
  if (!File)
    fatalError(std::format("cannot open file '{}'", Filename));

  auto Size = std::filesystem::file_size(Filename);
  std::vector<char> Data;
  Data.reserve(Size + 1);
  Data.resize(Size + 1);
  File.read(Data.data(), Size);
  Data[Size] = '\0';
  if (!File)
    fatalError(std::format("failed to read file '{}'", Filename));
  return std::make_unique<MemoryBuffer>(std::move(Data));
}

std::unique_ptr<MemoryBuffer> MemoryBuffer::fromStdin() {
  std::vector<char> Data;
  Data.reserve(4096);
  char Buf[4096];
  while (std::cin.good()) {
    std::cin.read(Buf, 4096);
    auto N = std::cin.gcount();
    Data.insert(Data.end(), Buf, Buf + N);
  }

  if (!std::cin.eof())
    fatalError("failed to read from stdin");

  Data.push_back('\0');
  return std::make_unique<MemoryBuffer>(std::move(Data));
}

std::unique_ptr<MemoryBuffer> MemoryBuffer::fromString(const std::string &Str) {
  std::vector<char> Data(Str.begin(), Str.end());
  return std::make_unique<MemoryBuffer>(std::move(Data));
}

void MemoryBuffer::skipUTF8BOM() {
  if (Data.size() >= 3 &&
      std::memcmp(Data.data(), "\xef\xbb\xbf", 3) == 0)
    Data.erase(Data.begin(), Data.begin() + 3);
}

// Replaces \r or \r\n with \n.
void MemoryBuffer::canonicalizeNewline() {
  if (Data.empty())
    return;

  // Ensure the buffer is null-terminated so we can scan with C-string logic.
  if (Data.back() != '\0')
    Data.push_back('\0');

  char *P = Data.data();
  int I = 0, J = 0;

  while (P[I]) {
    if (P[I] == '\r' && P[I + 1] == '\n') {
      I += 2;
      P[J++] = '\n';
    } else if (P[I] == '\r') {
      ++I;
      P[J++] = '\n';
    } else {
      P[J++] = P[I++];
    }
  }

  P[J] = '\0';
  Data.resize(static_cast<std::size_t>(J) + 1);
}

void MemoryBuffer::removeBackslashNewline() {
  if (Data.empty())
    return;

  // Ensure the buffer is null-terminated so we can scan with C-string logic.
  if (Data.back() != '\0')
    Data.push_back('\0');

  char *P = Data.data();
  int I = 0, J = 0;
  // Number of deleted continued newlines pending re-insertion.
  int N = 0;

  while (P[I]) {
    if (P[I] == '\\' && P[I + 1] == '\n') {
      I += 2;
      ++N;
    } else if (P[I] == '\n') {
      P[J++] = P[I++];
      for (; N > 0; --N)
        P[J++] = '\n';
    } else {
      P[J++] = P[I++];
    }
  }

  for (; N > 0; --N)
    P[J++] = '\n';
  P[J] = '\0';
  Data.resize(static_cast<std::size_t>(J) + 1);
}

// Replace \uXXXX / \UXXXXXXXX with the corresponding UTF-8 encoding.
void MemoryBuffer::convertUniversalChars() {
  if (Data.empty())
    return;

  if (Data.back() != '\0')
    Data.push_back('\0');

  char *P = Data.data();
  char *Q = P;

  while (*P) {
    if (startsWith(P, "\\u")) {
      std::uint32_t C = readUniversalChar(P + 2, 4);
      if (C) {
        P += 6;
        Q += encodeUTF8(Q, C);
      } else {
        *Q++ = *P++;
      }
    } else if (startsWith(P, "\\U")) {
      std::uint32_t C = readUniversalChar(P + 2, 8);
      if (C) {
        P += 10;
        Q += encodeUTF8(Q, C);
      } else {
        *Q++ = *P++;
      }
    } else if (P[0] == '\\') {
      *Q++ = *P++;
      *Q++ = *P++;
    } else {
      *Q++ = *P++;
    }
  }

  *Q = '\0';
  Data.resize(static_cast<std::size_t>(Q - Data.data()) + 1);
}

} // namespace rcc
