#include "Basic/SourceManager.h"
#include "Lex/Token.h"

#include <cassert>
#include <optional>

namespace rcc {

SourceLocation SourceManager::getLocForStartOfFile(FileID FID) const {
  const auto *FE = FileMgr.getFile(FID);
  if (!FE)
    return SourceLocation();

  return createLocation(FID, 0);
}

SourceLocation SourceManager::createBeginLocation(const Token *Tok) {
  assert(Tok->getLoc() >= CurrBegin);
  return createLocation(CurrFileID, Tok->getLoc() - CurrBegin);
}

SourceLocation SourceManager::createBeginLocation(const char *Loc) {
  assert(Loc >= CurrBegin && Loc <= CurrEnd);
  return createLocation(CurrFileID, Loc - CurrBegin);
}

SourceLocation SourceManager::createEndLocation(const Token *Tok) {
  assert(Tok->getLoc() >= CurrBegin);
  return createLocation(CurrFileID, Tok->getLoc() + Tok->getLen() - CurrBegin);
}

const char *SourceManager::getLoc(SourceLocation Loc) const {
  assert(Loc.isValid());
  if (Loc.getFileID() == CurrFileID)
    return CurrBegin + Loc.getOffset();

  FileID FID = Loc.getFileID();
  const auto *FE = FileMgr.getFile(FID);
  if (!FE)
    return nullptr;

  return FE->getContent()->getBufferStart() + Loc.getOffset();
}

std::string_view SourceManager::getFilename(SourceLocation Loc) const {
  const auto *FE = getFileEntry(Loc);
  if (!FE)
    return "";
  return FE->getPath();
}

const FileEntry *SourceManager::getFileEntry(SourceLocation Loc) const {
  FileID FID = Loc.getFileID();
  return FileMgr.getFile(FID);
}

std::optional<SourceLineInfo>
SourceManager::getLineInfo(SourceLocation Loc) const {
  const FileEntry *FE = getFileEntry(Loc);
  if (!FE)
    return std::nullopt;

  // TODO: Optimize the performance of retrieving line information.
  const char *FileBegin = FE->getContent()->getBufferStart();
  const char *TargetLoc = getLoc(Loc);

  unsigned LineNo = 1;
  const char *LineBegin = FileBegin;
  const char *P = FileBegin;
  while (P < TargetLoc) {
    if (*P == '\n') {
      ++LineNo;
      LineBegin = P + 1;
    }
    ++P;
  }

  const char *LineEnd = LineBegin;
  while (LineEnd < FE->getContent()->getBufferEnd() && *LineEnd != '\n')
    ++LineEnd;

  unsigned ColNo = TargetLoc - LineBegin + 1;
  std::string_view CodeLine(LineBegin, LineEnd - LineBegin);
  SourceLineInfo Info{LineNo, ColNo, CodeLine};
  return Info;
}

unsigned SourceManager::getLineNumber(SourceLocation Loc) const {
  return getLineInfo(Loc)->LineNo;
}

FileID SourceManager::createFileID(const std::string &Path) {
  const auto *FE = FileMgr.getFile(Path);
  if (!FE)
    return FileID::getFileID(0);

  auto FID = FileID::getFileID(FE->getUID());
  CurrBegin = FE->getContent()->getBufferStart();
  CurrEnd = FE->getContent()->getBufferEnd();
  CurrFileID = FID;
  return FID;
}

SourceLocation SourceManager::createLocation(FileID FID,
                                             std::uint64_t Offset) const {
  assert(Offset < (1ULL << NumBitsForOffset));
  auto FIDVal = static_cast<std::uint64_t>(FID.getOpaqueValue());
  std::uint64_t ID = (FIDVal << NumBitsForOffset) | Offset;
  return SourceLocation(ID);
}

} // namespace rcc