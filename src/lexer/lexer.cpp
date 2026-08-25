#include "lexer/lexer.h"

#include <cctype>
#include <unordered_set>

namespace mcdc {

namespace {

const std::unordered_set<std::string> &keywords() {
  static const std::unordered_set<std::string> kKeywords = {
      "model",      "end",     "equation", "parameter", "constant", "Real",
      "Integer",    "Boolean", "start",    "fixed",     "unit",     "annotation",
      "experiment", "true",    "false",    "and",       "or",       "not",
      "der",        "package", "import",   "algorithm", "when",     "if",
      "connect",    "reinit",  "record",   "function",  "each",
  };
  return kKeywords;
}

bool startsIdentifier(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
bool isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::tokenize(DiagnosticCollector &diags) const {
  std::vector<Token> tokens;
  int line = 1;
  int col = 1;
  const size_t n = source_.size();
  size_t i = 0;

  auto emit = [&](TokKind kind, std::string lexeme, int tokLine, int tokCol) {
    tokens.push_back(Token{kind, std::move(lexeme), tokLine, tokCol});
  };
  auto advance = [&](size_t count) {
    for (size_t k = 0; k < count && i < n; ++k) {
      if (source_[i] == '\n') {
        ++line;
        col = 1;
      } else {
        ++col;
      }
      ++i;
    }
  };

  while (i < n) {
    const char c = source_[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance(1);
      continue;
    }
    // 行注释
    if (c == '/' && i + 1 < n && source_[i + 1] == '/') {
      while (i < n && source_[i] != '\n')
        advance(1);
      continue;
    }
    // 块注释
    if (c == '/' && i + 1 < n && source_[i + 1] == '*') {
      const int startLine = line;
      advance(2);
      bool closed = false;
      while (i < n) {
        if (source_[i] == '*' && i + 1 < n && source_[i + 1] == '/') {
          advance(2);
          closed = true;
          break;
        }
        advance(1);
      }
      if (!closed) {
        diags.addError(Location{"" /*由调用方补全*/, startLine, col}, Code::ExprUnsupported,
                       "块注释未终止");
      }
      continue;
    }

    const int tokLine = line;
    const int tokCol = col;

    if (std::isdigit(static_cast<unsigned char>(c))) {
      size_t j = i;
      while (j < n && std::isdigit(static_cast<unsigned char>(source_[j])))
        ++j;
      bool isReal = false;
      if (j < n && source_[j] == '.') {
        isReal = true;
        ++j;
        while (j < n && std::isdigit(static_cast<unsigned char>(source_[j])))
          ++j;
      }
      if (j < n && (source_[j] == 'e' || source_[j] == 'E')) {
        size_t k2 = j + 1;
        if (k2 < n && (source_[k2] == '+' || source_[k2] == '-'))
          ++k2;
        if (k2 < n && std::isdigit(static_cast<unsigned char>(source_[k2]))) {
          isReal = true;
          j = k2;
          while (j < n && std::isdigit(static_cast<unsigned char>(source_[j])))
            ++j;
        }
      }
      std::string lexeme = source_.substr(i, j - i);
      advance(j - i);
      emit(isReal ? TokKind::Real : TokKind::Int, std::move(lexeme), tokLine, tokCol);
      continue;
    }

    if (startsIdentifier(c)) {
      size_t j = i;
      while (j < n && isIdentifierChar(source_[j]))
        ++j;
      std::string lexeme = source_.substr(i, j - i);
      advance(j - i);
      // 注意：先求值分类再移动 lexeme（函数实参求值顺序不可依赖）。
      const TokKind kind = isKeyword(lexeme) ? TokKind::Keyword : TokKind::Ident;
      emit(kind, std::move(lexeme), tokLine, tokCol);
      continue;
    }

    if (c == '"') {
      advance(1);
      size_t j = i;
      bool terminated = false;
      while (j < n) {
        if (source_[j] == '"') {
          terminated = true;
          break;
        }
        if (source_[j] == '\n')
          break;
        ++j;
      }
      if (!terminated) {
        diags.addError(Location{"", tokLine, tokCol}, Code::ExprUnsupported, "字符串字面量未终止");
        advance(j - i);
        continue;
      }
      std::string lexeme = source_.substr(i, j - i);
      advance(j - i);
      advance(1); // 收尾引号
      emit(TokKind::String, std::move(lexeme), tokLine, tokCol);
      continue;
    }

    // 多字符运算符优先：== <> <= >=
    if (i + 1 < n) {
      const std::string two = source_.substr(i, 2);
      if (two == "==" || two == "<>" || two == "<=" || two == ">=") {
        advance(2);
        emit(TokKind::Op, two, tokLine, tokCol);
        continue;
      }
    }
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')' || c == ',' ||
        c == ';' || c == '=' || c == '<' || c == '>') {
      advance(1);
      emit(TokKind::Op, std::string(1, c), tokLine, tokCol);
      continue;
    }

    diags.addError(Location{"", line, col}, Code::ExprUnsupported,
                   std::string("非法字符: '") + c + "'");
    advance(1);
  }

  tokens.push_back(Token{TokKind::End, "", line, col});
  return tokens;
}

bool isKeyword(const std::string &word) {
  return keywords().count(word) > 0;
}

} // namespace mcdc
