#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"

namespace rcc {

FileID SourceLocation::getFileID() const {
  // TODO: Handle macro source location.
  std::uint64_t FID = this->ID >> SourceManager::NumBitsForOffset;
  return FileID::getFileID(static_cast<int>(FID));
}

std::uint64_t SourceLocation::getOffset() const {
  std::uint64_t Mask = (1ULL << SourceManager::NumBitsForFileID) - 1;
  return ID & Mask;
}

} // namespace rcc