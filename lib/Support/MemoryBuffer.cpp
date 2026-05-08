#include "Support/MemoryBuffer.h"
#include "Support/Error.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace rcc {

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

} // namespace rcc