#ifndef RCC_BASIC_SOURCELOCATION_H
#define RCC_BASIC_SOURCELOCATION_H

#include <cstdint>

namespace rcc {

class SourceLocation {
public:
  constexpr SourceLocation() = default;
  SourceLocation(std::uint32_t ID) : ID(ID) {}

  bool isValid() const { return ID != 0; }
  bool isInvalid() const { return ID == 0; }

  SourceLocation getLocWithOffset(std::int32_t Offset) const {
    SourceLocation Loc(ID + Offset);
    return Loc;
  }

private:
  friend class SourceManager;

private:
  // For now, consider the ID as an offset.
  std::uint32_t ID = 0;
};

} // namespace rcc

#endif