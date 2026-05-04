#include "Basic/FileManager.h"
#include "Basic/FileEntry.h"
#include "Basic/SourceLocation.h"
#include "Support/Error.h"
#include "Support/MemoryBuffer.h"

#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <system_error>

namespace rcc {

const FileEntry *FileManager::getFile(const std::string &Path) {
  FileID FID = translateFileID(Path);
  auto &FE = FileEntries[FID];
  if (FE)
    return FE.get();

  std::unique_ptr<MemoryBuffer> Buf;
  if (Path == "-")
    Buf = MemoryBuffer::fromStdin();
  else
    Buf = MemoryBuffer::fromFile(Path);

  FE = std::make_unique<FileEntry>(Path, std::move(Buf), FID.getOpaqueValue());
  return FE.get();
}

const FileEntry *FileManager::getFile(FileID FID) {
  auto It = FileEntries.find(FID);
  if (It != FileEntries.end())
    return It->second.get();
  return nullptr;
}

const FileEntry *FileManager::getFileContaining(const char *Loc) const {
  auto Address = reinterpret_cast<std::uintptr_t>(Loc);
  for (const auto &[FID, FE] : FileEntries) {
    const MemoryBuffer *Buf = FE->getContent();
    auto Begin = reinterpret_cast<std::uintptr_t>(Buf->getBufferStart());
    auto End = reinterpret_cast<std::uintptr_t>(Buf->getBufferEnd());
    if (Begin <= Address && Address < End)
      return FE.get();
  }
  return nullptr;
}

FileID FileManager::translateFileID(const std::string &Path) {
  namespace fs = std::filesystem;
  std::error_code EC;
  fs::path P = fs::weakly_canonical(fs::path(Path), EC);
  if (EC) {
    fatalError(std::format("failed to canonicalize path '{}': {}", Path,
                           EC.message()));
  }

  std::string CanonicalPath = P.string();
  auto It = FileIDs.find(CanonicalPath);
  if (It != FileIDs.end())
    return It->second;

  FileID FID = FileID::getFileID(NextFileID++);
  FileIDs.emplace(CanonicalPath, FID);
  return FID;
}

} // namespace rcc