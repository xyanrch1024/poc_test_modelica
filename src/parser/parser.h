#pragma once
#include <optional>
#include <string>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "lexer/lexer.h"

namespace mcdc {

// 递归下降解析器（contracts/subset-grammar.md）。
// panic-mode 错误恢复：语句级错误报告后同步到下一个 ';' 继续，实现"一次报全"。
class Parser {
public:
  Parser(std::vector<Token> tokens, DiagnosticCollector &diags);

  // 解析整个文件；文件为空/仅注释时报 MC0001 并返回 nullopt。
  std::optional<ast::Model> parseFile();

private:
  bool atEnd() const;
  const Token &peek(size_t offset = 0) const;
  const Token &advance();
  bool check(TokKind kind, const std::string &lexeme) const;
  bool match(TokKind kind, const std::string &lexeme);
  bool expect(TokKind kind, const std::string &lexeme, const char *what);
  void reportUnsupported(const Token &tok);
  // 同步到 ';' 之后（panic-mode）。
  void synchronizeStatement();

  std::optional<ast::Component> parseComponent();
  std::optional<ast::Equation> parseEquation();
  std::optional<ast::Equation> parseIfEquation();
  ast::EquationSide parseEquationSide();
  ast::ExprPtr parseExpression(); // logical_or
  ast::ExprPtr parseLogicalOr();
  ast::ExprPtr parseLogicalAnd();
  ast::ExprPtr parseNot();
  ast::ExprPtr parseComparison();
  ast::ExprPtr parseAdditive();
  ast::ExprPtr parseMultiplicative();
  ast::ExprPtr parseUnary();
  ast::ExprPtr parsePrimary();
  ast::ExprPtr parseIfExpression();
  // 是否处于分支方程列表终止点（elseif/else/end）。
  bool atIfBranchEnd() const;
  std::optional<ast::Experiment> parseExperimentAnnotation();

  std::vector<Token> tokens_;
  size_t pos_ = 0;
  DiagnosticCollector &diags_;
  // if 构造递归深度护栏（T032）。
  static constexpr int kMaxIfDepth = 1024;
  int ifDepth_ = 0;
};

} // namespace mcdc
