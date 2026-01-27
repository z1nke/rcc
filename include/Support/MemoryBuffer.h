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

  explicit MemoryBuffer(std::vector<char> Data) : Data(std::move(Data)) {}

private:
  std::vector<char> Data;
};

} // namespace rcc

#endif