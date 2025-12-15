#ifndef RCC_BASIC_SOURCEMANAGER_H
#define RCC_BASIC_SOURCEMANAGER_H

#include "Basic/SourceLocation.h"
namespace rcc {

class Token;
class SourceManager {
public:
  SourceManager(const char *Begin) : Begin(Begin) {}

  const char *getBegin() const { return Begin; }
  const char *getLoc(SourceLocation Loc) const;

  SourceLocation createBeginLocation(const Token *Tok);
  SourceLocation createEndLocation(const Token *Tok);

private:
  const char *Begin = nullptr;
};

} // namespace rcc

#endif