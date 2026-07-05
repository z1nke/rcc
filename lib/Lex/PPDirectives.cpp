#include "Basic/Diagnostic.h"
#include "Basic/FileEntry.h"
#include "Basic/SourceManager.h"
#include "Lex/Lexer.h"
#include "Lex/MacroInfo.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"

#include <cctype>
#include <cstring>
#include <ctime>
#include <format>
#include <string>
#include <utility>

namespace rcc {

namespace {

// Format as in C11 6.10.8.1: "Mmm dd yyyy" (day is space-padded).
static std::string formatDate(const std::tm *Tm) {
  static constexpr const char *Mon[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };
  return std::format("\"{} {:>2} {}\"", Mon[Tm->tm_mon], Tm->tm_mday,
                     Tm->tm_year + 1900);
}

// Format as in C11 6.10.8.1: "hh:mm:ss".
static std::string formatTime(const std::tm *Tm) {
  return std::format("\"{:02}:{:02}:{:02}\"", Tm->tm_hour, Tm->tm_min,
                     Tm->tm_sec);
}

} // namespace

bool Preprocessor::isMacroIdentifier(const Token *Tok) {
  if (Tok->getLen() == 0)
    return false;

  unsigned char First = static_cast<unsigned char>(Tok->getLoc()[0]);
  if (!std::isalpha(First) && First != '_')
    return false;

  for (int I = 1; I < Tok->getLen(); ++I) {
    unsigned char C = static_cast<unsigned char>(Tok->getLoc()[I]);
    if (!std::isalnum(C) && C != '_')
      return false;
  }
  return true;
}

void Preprocessor::defineMacro(const char *Name, const char *Body) {
  std::size_t Len = std::strlen(Body);
  char *Buf =
      static_cast<char *>(MacroTokenAlloc.allocate(Len + 1, alignof(char)));
  std::memcpy(Buf, Body, Len + 1);

  MacroInfo MI;
  for (Token *Tok = Lex.tokenize(Buf); Tok->isNot(Token::TK_EOF);
       Tok = Tok->getNext())
    MI.addTokenToBody(*Tok);

  Macros.insert_or_assign(Name, std::move(MI));
}

void Preprocessor::defineCommandLineMacro(const std::string &Def) {
  std::size_t Eq = Def.find('=');
  if (Eq == std::string::npos) {
    defineMacro(Def.c_str(), "1");
    return;
  }

  std::string Name = Def.substr(0, Eq);
  defineMacro(Name.c_str(), Def.c_str() + Eq + 1);
}

void Preprocessor::undefMacro(const std::string &Name) { Macros.erase(Name); }

void Preprocessor::applyCommandLineMacros() {
  for (const CommandLineMacro &M : CommandLineMacros) {
    if (M.Action == CommandLineMacro::Undef)
      undefMacro(M.Text);
    else
      defineCommandLineMacro(M.Text);
  }
}

void Preprocessor::addBuiltin(const char *Name, BuiltinMacroFn Fn) {
  MacroInfo MI;
  MI.setHandler(Fn);
  Macros.insert_or_assign(Name, std::move(MI));
}

void Preprocessor::initMacros() {
  defineMacro("_LP64", "1");
  defineMacro("__C99_MACRO_WITH_VA_ARGS", "1");
  defineMacro("__ELF__", "1");
  defineMacro("__LP64__", "1");
  defineMacro("__SIZEOF_DOUBLE__", "8");
  defineMacro("__SIZEOF_FLOAT__", "4");
  defineMacro("__SIZEOF_INT__", "4");
  defineMacro("__SIZEOF_LONG_DOUBLE__", "16");
  defineMacro("__SIZEOF_LONG_LONG__", "8");
  defineMacro("__SIZEOF_LONG__", "8");
  defineMacro("__SIZEOF_POINTER__", "8");
  defineMacro("__SIZEOF_PTRDIFF_T__", "8");
  defineMacro("__SIZEOF_SHORT__", "2");
  defineMacro("__SIZEOF_SIZE_T__", "8");
  defineMacro("__SIZE_TYPE__", "unsigned long");
  defineMacro("__STDC_HOSTED__", "1");
  defineMacro("__STDC_NO_ATOMICS__", "1");
  defineMacro("__STDC_NO_COMPLEX__", "1");
  defineMacro("__STDC_UTF_16__", "1");
  defineMacro("__STDC_UTF_32__", "1");
  defineMacro("__STDC_VERSION__", "201112L");
  defineMacro("__STDC__", "1");
  defineMacro("__USER_LABEL_PREFIX__", "");
  defineMacro("__alignof__", "_Alignof");
  defineMacro("__rcc__", "1");
  defineMacro("__const__", "const");
  defineMacro("__gnu_linux__", "1");
  defineMacro("__inline__", "inline");
  defineMacro("__linux", "1");
  defineMacro("__linux__", "1");
  defineMacro("__signed__", "signed");
  defineMacro("__typeof__", "typeof");
  defineMacro("__unix", "1");
  defineMacro("__unix__", "1");
  defineMacro("__volatile__", "volatile");
  defineMacro("linux", "1");
  defineMacro("unix", "1");
  defineMacro("__riscv_mul", "1");
  defineMacro("__riscv_muldiv", "1");
  defineMacro("__riscv_fdiv", "1");
  defineMacro("__riscv_xlen", "64");
  defineMacro("__riscv", "1");
  defineMacro("__riscv64", "1");
  defineMacro("__riscv_div", "1");
  defineMacro("__riscv_float_abi_double", "1");
  defineMacro("__riscv_flen", "64");

  addBuiltin("__FILE__", &Preprocessor::handleFileMacro);
  addBuiltin("__LINE__", &Preprocessor::handleLineMacro);
  addBuiltin("__COUNTER__", &Preprocessor::handleCounterMacro);
  addBuiltin("__TIMESTAMP__", &Preprocessor::handleTimestampMacro);
  addBuiltin("__BASE_FILE__", &Preprocessor::handleBaseFileMacro);

  // [221] Add __DATE__ and __TIME__ macros
  std::time_t Now = std::time(nullptr);
  std::tm *Tm = std::localtime(&Now);
  defineMacro("__DATE__", formatDate(Tm).c_str());
  defineMacro("__TIME__", formatTime(Tm).c_str());

  applyCommandLineMacros();
}

void Preprocessor::handleDefineDirective(Token *&Rest, Token *NameTok) {
  if (!isMacroIdentifier(NameTok))
    Diag.fatalAt(NameTok->getLoc(), "macro name must be an identifier");

  MacroInfo MI;
  Token *Tok = NameTok->getNext();
  if (Tok->is(Token::TK_LParen) &&
      NameTok->getLoc() + NameTok->getLen() == Tok->getLoc()) {
    MI.setIsFunctionLike();
    Tok = Tok->getNext();
    while (Tok->isNot(Token::TK_RParen)) {
      if (Tok->is(Token::TK_Ellipsis)) {
        MI.setIsVariadic();
        MI.addParameter("__VA_ARGS__");
        Tok = Tok->getNext();
        if (Tok->isNot(Token::TK_RParen))
          Diag.fatalAt(Tok->getLoc(), "expected ')'");
        break;
      }

      if (!isMacroIdentifier(Tok))
        Diag.fatalAt(Tok->getLoc(), "expected an identifier");

      // [GNU] Support GCC-style variadic macro: name...
      if (Tok->getNext()->is(Token::TK_Ellipsis)) {
        MI.setIsVariadic();
        MI.addParameter(std::string(Tok->getLoc(), Tok->getLen()));
        Tok = Tok->getNext()->getNext();
        if (Tok->isNot(Token::TK_RParen))
          Diag.fatalAt(Tok->getLoc(), "expected ')'");
        break;
      }

      MI.addParameter(std::string(Tok->getLoc(), Tok->getLen()));
      Tok = Tok->getNext();
      if (Tok->is(Token::TK_RParen))
        break;
      if (Tok->isNot(Token::TK_Comma))
        Diag.fatalAt(Tok->getLoc(), "expected ','");
      Tok = Tok->getNext();
    }
    Tok = Tok->getNext();
  }

  while (Tok->isNot(Token::TK_EOF) && !Tok->isAtStartOfLine()) {
    MI.addTokenToBody(*Tok);
    Tok = Tok->getNext();
  }

  std::string Name(NameTok->getLoc(), NameTok->getLen());
  Macros.insert_or_assign(std::move(Name), std::move(MI));
  Rest = Tok;
}

void Preprocessor::handleUndefDirective(Token *&Rest, Token *NameTok) {
  if (!isMacroIdentifier(NameTok))
    Diag.fatalAt(NameTok->getLoc(), "macro name must be an identifier");

  std::string Name(NameTok->getLoc(), NameTok->getLen());
  Macros.erase(Name);

  Token *Tok = NameTok->getNext();
  if (Tok->isNot(Token::TK_EOF) && !Tok->isAtStartOfLine()) {
    Diag.warnAt(Tok->getLoc(), "extra token");
    do {
      Tok = Tok->getNext();
    } while (Tok->isNot(Token::TK_EOF) && !Tok->isAtStartOfLine());
  }
  Rest = Tok;
}

// #line lineno ["filename"]
void Preprocessor::handleLineDirective(Token *&Rest, Token *Tok) {
  Token *Start = Tok;
  Token *Expanded = expandMacroExpression(Rest, Tok);
  Lex.convertPPTokens(Expanded);

  if (Expanded->isNot(Token::TK_Num) ||
      Expanded->getNumericLiteralKind() != Token::NumericLiteralKind::Int)
    Diag.fatalAt(Expanded->getLoc(), "invalid line marker");

  SourceManager &SM = Diag.getSourceManager();
  SourceLocation Loc = SM.createBeginLocation(Start);
  FileEntry *FE = SM.getFileEntry(Loc);

  // LineDelta is relative to the physical line of the marker arguments.
  unsigned Presumed = SM.getLineNumber(Loc);
  int Physical = static_cast<int>(Presumed) - FE->getLineDelta();
  FE->setLineDelta(static_cast<int>(Expanded->getVal()) - Physical);

  Token *Next = Expanded->getNext();
  if (Next->is(Token::TK_EOF) || Next->isAtStartOfLine())
    return;

  if (Next->isNot(Token::TK_StrLiteral))
    Diag.fatalAt(Next->getLoc(), "filename expected");

  FE->setDisplayName(Next->getStringLiteral(Diag));
}

} // namespace rcc
