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
  Data.reserve(Size);
  Data.resize(Size);
  File.read(Data.data(), Size);
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

  return std::make_unique<MemoryBuffer>(std::move(Data));
}

std::unique_ptr<MemoryBuffer> MemoryBuffer::fromString(const std::string &Str) {
  std::vector<char> Data(Str.begin(), Str.end());
  return std::make_unique<MemoryBuffer>(std::move(Data));
}

} // namespace rcc