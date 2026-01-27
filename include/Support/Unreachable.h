#ifndef RCC_SUPPORT_UNREACHABLE_HPP
#define RCC_SUPPORT_UNREACHABLE_HPP

namespace rcc {
namespace details {

[[noreturn]] void unreachableInternal(const char *Msg, const char *Filename,
                                      unsigned Lineno);

} // namespace details
} // namespace rcc

#define RCC_UNREACHABLE(msg)                                                   \
  rcc::details::unreachableInternal(msg, __FILE__, __LINE__)

#endif