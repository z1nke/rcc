#include "Basic/SourceManager.h"
#include "Lex/Token.h"

#include <cassert>

namespace rcc {

SourceLocation SourceManager::createBeginLocation(const Token *Tok) {
  assert(Tok->getLoc() >= Begin);
  return SourceLocation(Tok->getLoc() - Begin + 1);
}

SourceLocation SourceManager::createEndLocation(const Token *Tok) {
  assert(Tok->getLoc() >= Begin);
  return SourceLocation(Tok->getLoc() + Tok->getLen() - Begin);
}

const char *SourceManager::getLoc(SourceLocation Loc) const {
  assert(Loc.isValid());
  return Begin + Loc.ID - 1;
}

} // namespace rcc