#ifndef RCC_SUPPORT_MEMORYBUFFER_H
#define RCC_SUPPORT_MEMORYBUFFER_H

#include <memory>
#include <vector>

namespace rcc {

class MemoryBuffer {
public:
  static std::unique_ptr<MemoryBuffer> fromFile(const std::string &Filename);
  static std::unique_ptr<MemoryBuffer> fromStdin();
  static std::unique_ptr<MemoryBuffer> fromString(const std::string &Str);

  const char *getBufferStart() const { return Data.data(); }
  const char *getBufferEnd() const { return Data.data() + Data.size(); }
  std::size_t getBufferSize() const { return Data.size(); }

  std::string_view getBuffer() const {
    return std::string_view(Data.data(), Data.size());
  }

  // Replaces \r or \r\n with \n.
  void canonicalizeNewline();

  // Replace \uXXXX / \UXXXXXXXX with the corresponding UTF-8 bytes.
  void convertUniversalChars();

  // Translation phase 2: delete backslash-newline pairs, preserving line
  // numbers by re-inserting the skipped newlines at the next real newline.
  void removeBackslashNewline();

  explicit MemoryBuffer(std::vector<char> Data) : Data(std::move(Data)) {}

private:
  std::vector<char> Data;
};

} // namespace rcc

#endif