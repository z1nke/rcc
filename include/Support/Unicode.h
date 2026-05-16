#ifndef RCC_SUPPORT_UNICODE_H
#define RCC_SUPPORT_UNICODE_H

#include <cstdint>

namespace rcc {

class Diagnostic;

/// Encode Unicode code point \p C as UTF-8 into \p Buf.
/// Returns the number of bytes written (1–4).
int encodeUTF8(char *Buf, std::uint32_t C);

/// Decode one UTF-8 code point starting at \p P.
/// On success, sets \p *NewPos to the next byte and returns the code point.
std::uint32_t decodeUTF8(const char **NewPos, const char *P, Diagnostic &Diag);

} // namespace rcc

#endif
