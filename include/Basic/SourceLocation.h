#ifndef RCC_BASIC_SOURCELOCATION_H
#define RCC_BASIC_SOURCELOCATION_H

#include <cstdint>
#include <functional>

namespace rcc {

class FileID {
public:
  static FileID getFileID(int ID) {
    FileID FID;
    FID.ID = ID;
    return FID;
  }

  bool isValid() const { return ID != 0; }
  bool isInvalid() const { return ID == 0; }
  auto operator<=>(const FileID &) const = default;
  int getOpaqueValue() const { return ID; }

private:
  int ID = 0;
};

class SourceLocation {
public:
  constexpr SourceLocation() = default;
  SourceLocation(std::uint64_t ID) : ID(ID) {}

  bool isValid() const { return ID != 0; }
  bool isInvalid() const { return ID == 0; }

  SourceLocation getLocWithOffset(int Offset) const {
    SourceLocation Loc(ID + Offset);
    return Loc;
  }

  FileID getFileID() const;
  std::uint64_t getOffset() const;

private:
  friend class SourceManager;

private:
  // +-+---------------+------------------------------------------------+
  // |1|      15       |                     48                         |
  // +-+---------------+------------------------------------------------+
  // | |   FileID      |                   Offset                       |
  std::uint64_t ID = 0;
};

} // namespace rcc

namespace std {
template <> struct hash<rcc::FileID> {
  size_t operator()(const rcc::FileID &FID) const noexcept {
    return FID.getOpaqueValue();
  }
};
} // namespace std

#endif