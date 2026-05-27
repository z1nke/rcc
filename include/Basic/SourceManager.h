#ifndef RCC_BASIC_SOURCEMANAGER_H
#define RCC_BASIC_SOURCEMANAGER_H

#include "Basic/FileEntry.h"
#include "Basic/FileManager.h"
#include "Basic/SourceLocation.h"

namespace rcc {

class Diagnostic;
class Token;

struct SourceLineInfo {
  unsigned LineNo;
  unsigned ColNo;
  std::string_view LineContent;
};

class SourceManager {
public:
  SourceManager(FileManager &FileMgr) : FileMgr(FileMgr) {}

  SourceLocation getLocForStartOfFile(FileID FID) const;

  const char *getLoc(SourceLocation Loc) const;

  std::string_view getFilename(SourceLocation Loc) const;
  const FileEntry *getFileEntry(SourceLocation Loc) const;
  FileEntry *getFileEntry(SourceLocation Loc);
  std::optional<SourceLineInfo> getLineInfo(SourceLocation Loc) const;
  unsigned getLineNumber(SourceLocation Loc) const;

  SourceLocation createBeginLocation(const Token *Tok);
  SourceLocation createBeginLocation(const char *Loc);
  SourceLocation createEndLocation(const Token *Tok);

  FileManager &getFileManager() { return FileMgr; }

  FileID createFileID(const std::string &Path);

  static constexpr unsigned NumBitsForFileID = 15;
  static constexpr unsigned NumBitsForOffset = 48;

private:
  SourceLocation createLocation(FileID FID, std::uint64_t Offset) const;

private:
  FileManager &FileMgr;
};

} // namespace rcc

#endif