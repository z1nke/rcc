#include "Basic/Diagnostic.h"
#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"
#include "Support/Unicode.h"

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
  // ColNo is 1-based byte offset; convert to display columns for the caret.
  int ByteOff = static_cast<int>(LineInfo->ColNo) - 1;
  int Width = displayWidth(LineInfo->LineContent.data(), ByteOff);
  std::print(stderr, "{:>{}}^ ", "", LineInfoStr.size() + Width);
}

} // namespace rcc
