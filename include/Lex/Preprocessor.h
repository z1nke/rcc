#ifndef RCC_LEX_PREPROCESSOR_H
#define RCC_LEX_PREPROCESSOR_H

namespace rcc {

class Diagnostic;
class Token;

class Preprocessor {
public:
  explicit Preprocessor(Diagnostic &Diag) : Diag(Diag) {}

  Token *preprocess(Token *Toks);

private:
  Diagnostic &Diag;
};

} // namespace rcc

#endif
