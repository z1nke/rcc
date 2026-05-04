#ifndef RCC_LEX_PREPROCESSOR_H
#define RCC_LEX_PREPROCESSOR_H

namespace rcc {

class Token;

class Preprocessor {
public:
  Token *preprocess(Token *Toks);
};

} // namespace rcc

#endif
