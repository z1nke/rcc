#ifndef RCC_BASIC_UNREACHABLE_HPP
#define RCC_BASIC_UNREACHABLE_HPP

namespace rcc {
namespace details {

[[noreturn]] void unreachableInternal(const char *msg, const char *filename,
                                      unsigned lineno);

} // namespace details
} // namespace rcc

#define RCC_UNREACHABLE(msg)                                                   \
  rcc::details::unreachableInternal(msg, __FILE__, __LINE__)

#endif