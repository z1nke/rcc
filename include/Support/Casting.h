#ifndef RCC_SUPPORT_CASTING_H
#define RCC_SUPPORT_CASTING_H

#include <cassert>
#include <type_traits>

namespace rcc {

template <typename To, typename From> inline bool isa(const From &Val) {
  return std::is_base_of<To, From>::value || To::classof(Val);
}

template <typename First, typename Second, typename... Rest, typename Y>
inline bool isa(const Y &Val) {
  return isa<First>(Val) || isa<Second, Rest...>(Val);
}

template <typename To, typename From>
inline const To *dynCast(const From *Val) {
  return isa<To>(Val) ? static_cast<const To *>(Val) : nullptr;
}

template <typename To, typename From> inline To *dynCast(From *Val) {
  return isa<To>(Val) ? static_cast<To *>(Val) : nullptr;
}

template <typename To, typename From>
inline const To *dynCastOrNull(const From *Val) {
  return Val ? dynCast<To>(Val) : nullptr;
}

template <typename To, typename From> inline To *dynCastOrNull(From *Val) {
  return Val ? dynCast<To>(Val) : nullptr;
}

template <typename To, typename From> inline const To *cast(const From *Val) {
  assert(isa<To>(Val));
  return static_cast<const To *>(Val);
}

template <typename To, typename From> inline To *cast(From *Val) {
  assert(isa<To>(Val));
  return static_cast<To *>(Val);
}

} // namespace rcc

#endif