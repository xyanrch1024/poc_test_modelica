#pragma once
#include <string>
#include <vector>

#include "diagnostics/diagnostic.h"

namespace mcdc {

enum class TokKind { Ident, Keyword, Int, Real, String, Op, End };

struct Token {
  TokKind kind = TokKind::End;
  std::string lexeme;
  int line = 0;
  int col = 0;
};

// 词法分析器：子集文法的词法面（contracts/subset-grammar.md）。
// 行列号自 1 起始，指向 token 首字符；词法错误经 DiagnosticCollector 上报（MC0201）。
class Lexer {
public:
  explicit Lexer(std::string source);

  std::vector<Token> tokenize(DiagnosticCollector &diags) const;

private:
  std::string source_;
};

bool isKeyword(const std::string &word);

} // namespace mcdc
