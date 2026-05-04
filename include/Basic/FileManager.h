#ifndef RCC_BASIC_FILEMANAGER_H
#define RCC_BASIC_FILEMANAGER_H

#include <unordered_map>

#include "Basic/FileEntry.h"
#include "Basic/SourceLocation.h"

namespace rcc {

class FileManager {
public:
  FileManager() = default;

  const FileEntry *getFile(const std::string &Path);
  const FileEntry *getFile(FileID FID);
  const FileEntry *getFileContaining(const char *Loc) const;
  FileID translateFileID(const std::string &Path);

private:
  std::unordered_map<FileID, std::unique_ptr<FileEntry>> FileEntries;
  std::unordered_map<std::string, FileID> FileIDs;
  int NextFileID = 1;
};

} // namespace rcc

#endif