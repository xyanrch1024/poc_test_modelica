#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"

namespace mcdc {

struct SymbolInfo {
  std::string name;
  ast::Component::Kind kind;
  ast::Component::DeclType type;
};

// 符号表（data-model.md）：按声明序保存；重复声明 MC0101；constant 缺初值 MC0103。
class SymbolTable {
public:
  static std::optional<SymbolTable> build(const ast::Model &model, DiagnosticCollector &diags);

  const SymbolInfo *find(const std::string &name) const;
  const std::vector<SymbolInfo> &all() const { return symbols_; }

private:
  std::vector<SymbolInfo> symbols_;
  std::unordered_map<std::string, size_t> index_;
};

// 表达式遍历检查：MC0102 未声明、MC0202 白名单外函数、MC0203 非法 der 位置。
// 注意：方程左侧顶层 der(Ident) 是合法形态，由调用方跳过，不在本函数检查范围内。
void analyzeExpressions(const ast::Model &model, const SymbolTable &table,
                        DiagnosticCollector &diags);

// "是否为布尔表达式"的轻量判定（data-model.md §isBooleanExpr）：
// 布尔字面量、not/and/or、关系比较、布尔标识符、布尔型 if 表达式。
// 用于 if 条件位置校验（MC0204）。
bool isBooleanExpr(const ast::Expr &expr, const SymbolTable &table);

// 表达式"类型类"判定：Numeric / Boolean / Unknown（用于 if 分支一致性 MC0205）。
enum class ExprTypeClass { Numeric, Boolean, Unknown };
ExprTypeClass typeClassOf(const ast::Expr &expr, const SymbolTable &table);

} // namespace mcdc
