#ifndef RCC_BASIC_FILEENTRY_H
#define RCC_BASIC_FILEENTRY_H

#include "Support/MemoryBuffer.h"
#include <memory>

namespace rcc {

class FileEntry {
public:
  FileEntry(std::string Path, std::unique_ptr<MemoryBuffer> Content, int UID)
      : Path(std::move(Path)), Content(std::move(Content)),
        Size(this->Content->getBufferSize()), UID(UID) {}

  const std::string &getPath() const { return Path; }
  std::size_t getSize() const { return Size; }
  const MemoryBuffer *getContent() const { return Content.get(); }
  int getUID() const { return UID; }

private:
  std::string Path;
  std::unique_ptr<MemoryBuffer> Content;
  std::size_t Size = 0;
  int UID = 0;
};

} // namespace rcc

#endif