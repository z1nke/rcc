#ifndef RCC_BASIC_LINKAGE_H
#define RCC_BASIC_LINKAGE_H

namespace rcc {

enum class Linkage : unsigned char {
  /// No linkage: the entity is unique to its scope.
  NoLinkage = 0,

  /// Internal linkage: visible within this translation unit only.
  InternalLinkage,

  /// External linkage: visible from other translation units.
  ExternalLinkage,
};

inline bool isExternallyVisible(Linkage L) {
  return L == Linkage::ExternalLinkage;
}

} // namespace rcc

#endif
