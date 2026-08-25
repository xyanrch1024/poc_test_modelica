#include "semantic/symbols.h"

#include <functional>
#include <unordered_set>

namespace mcdc {

namespace {

const std::unordered_set<std::string> &kBuiltinFunctions() {
  static const std::unordered_set<std::string> kSet = {
      "abs", "sqrt", "sin", "cos", "tan", "exp", "log", "log10", "min", "max",
  };
  return kSet;
}

void walkExpr(const ast::Expr &expr, const SymbolTable &table, DiagnosticCollector &diags) {
  switch (expr.kind) {
  case ast::ExprKind::Ident: {
    if (table.find(expr.token.lexeme) == nullptr) {
      diags.addError(Location{"", expr.token.line, expr.token.col}, Code::SymbolUndeclared,
                     "使用未声明标识符 \"" + expr.token.lexeme + "\"");
    }
    break;
  }
  case ast::ExprKind::NumLit:
  case ast::ExprKind::BoolLit:
    break;
  case ast::ExprKind::Der:
    // 嵌套在表达式树中的 der —— 仅允许方程左侧顶层出现（MC0203）。
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprBadDerPlacement,
                   "der(...) 只能出现在方程左侧");
    if (expr.lhs)
      walkExpr(*expr.lhs, table, diags);
    break;
  case ast::ExprKind::Unary:
    if (expr.lhs)
      walkExpr(*expr.lhs, table, diags);
    break;
  case ast::ExprKind::Binary:
    if (expr.lhs)
      walkExpr(*expr.lhs, table, diags);
    if (expr.rhs)
      walkExpr(*expr.rhs, table, diags);
    break;
  case ast::ExprKind::Call: {
    if (kBuiltinFunctions().count(expr.op) == 0) {
      diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnknownFunction,
                     "调用了白名单之外的函数 \"" + expr.op + "\"");
    }
    for (const auto &arg : expr.args) {
      if (arg)
        walkExpr(*arg, table, diags);
    }
    break;
  }
  }
}

} // namespace

std::optional<SymbolTable> SymbolTable::build(const ast::Model &model, DiagnosticCollector &diags) {
  SymbolTable table;
  std::unordered_map<std::string, size_t> index;
  bool ok = true;

  for (const auto &comp : model.components) {
    const std::string &name = comp.nameTok.lexeme;
    if (index.count(name)) {
      diags.addError(Location{"", comp.nameTok.line, comp.nameTok.col}, Code::SymbolDuplicate,
                     "标识符重复声明 \"" + name + "\"");
      ok = false;
      continue;
    }
    if (comp.kind == ast::Component::Kind::Constant && !comp.value) {
      diags.addError(Location{"", comp.nameTok.line, comp.nameTok.col},
                     Code::SymbolConstantMissingValue, "constant 缺少初始化值");
      ok = false;
    }
    index[name] = table.symbols_.size();
    table.symbols_.push_back(SymbolInfo{name, comp.kind, comp.type});
  }

  table.index_ = std::move(index);
  if (!ok)
    return std::nullopt;
  return table;
}

const SymbolInfo *SymbolTable::find(const std::string &name) const {
  auto it = index_.find(name);
  if (it == index_.end())
    return nullptr;
  return &symbols_[it->second];
}

void analyzeExpressions(const ast::Model &model, const SymbolTable &table,
                        DiagnosticCollector &diags) {
  for (const auto &eq : model.equations) {
    // 右侧整体检查；左侧仅在非 der 形态时检查（顶层 der 合法）。
    if (!eq.rhs.isDer && eq.rhs.expr)
      walkExpr(*eq.rhs.expr, table, diags);
    if (!eq.lhs.isDer && eq.lhs.expr)
      walkExpr(*eq.lhs.expr, table, diags);
  }
  // 组件初始化表达式同样需要检查（如 parameter Real k = kappa;）。
  for (const auto &comp : model.components) {
    if (comp.value)
      walkExpr(*comp.value, table, diags);
    if (comp.start)
      walkExpr(*comp.start, table, diags);
    if (comp.fixed)
      walkExpr(*comp.fixed, table, diags);
  }
}

} // namespace mcdc
