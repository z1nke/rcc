#ifndef RCC_SUPPORT_ERROR_H
#define RCC_SUPPORT_ERROR_H

namespace rcc {

[[noreturn]] void errorf(const char *Fmt, ...);

} // namespace rcc

#endif