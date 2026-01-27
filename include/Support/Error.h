#ifndef RCC_SUPPORT_ERROR_H
#define RCC_SUPPORT_ERROR_H

#include <string>

namespace rcc {

void fatalError(const char *Msg);
void fatalError(const std::string &Msg);
void printErrorWithColor();

} // namespace rcc

#endif