#include "Basic/Diagnostic.h"
#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"

#include <print>

namespace rcc {

void Diagnostic::printLoc(SourceLocation Loc) const {
  if (Loc.isInvalid())
    return;

  std::string_view Filename = SM.getFilename(Loc);
  auto LineInfo = SM.getLineInfo(Loc);
  if (!LineInfo)
    return;

  auto LineInfoStr = std::format("{}:{}: ", Filename, LineInfo->LineNo);
  std::println(stderr, "{}{}", LineInfoStr, LineInfo->LineContent);
  std::print(stderr, "{:>{}}^ ", "", LineInfoStr.size() + LineInfo->ColNo - 1);
}

} // namespace rcc
