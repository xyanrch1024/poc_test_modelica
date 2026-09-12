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
  case ast::ExprKind::If:
    // 子表达式全遍历（MC0102/0202/0203 不漏检）；随后做条件类型与分支一致性校验。
    if (expr.cond)
      walkExpr(*expr.cond, table, diags);
    if (expr.thenExpr)
      walkExpr(*expr.thenExpr, table, diags);
    if (expr.elseExpr)
      walkExpr(*expr.elseExpr, table, diags);
    if (expr.cond && !isBooleanExpr(*expr.cond, table)) {
      diags.addError(Location{"", expr.cond->token.line, expr.cond->token.col},
                     Code::ExprIfConditionNotBoolean, "if 条件必须是布尔表达式");
    }
    {
      const auto t = expr.thenExpr ? typeClassOf(*expr.thenExpr, table) : ExprTypeClass::Unknown;
      const auto f = expr.elseExpr ? typeClassOf(*expr.elseExpr, table) : ExprTypeClass::Unknown;
      if (t != ExprTypeClass::Unknown && f != ExprTypeClass::Unknown && t != f) {
        const Token &at = expr.thenExpr ? expr.thenExpr->token : expr.token;
        diags.addError(Location{"", at.line, at.col}, Code::ExprIfBranchTypeMismatch,
                       "if 表达式分支类型不一致（数值与布尔不能混用）");
      }
    }
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

// 递归遍历单个方程（可为条件方程）的全部表达式；条件方程的条件位置做 MC0204 校验。
void walkEquationExpr(const ast::Equation &eq, const SymbolTable &table,
                      DiagnosticCollector &diags) {
  if (eq.isIf) {
    for (const auto &branch : eq.branches) {
      if (branch.condition) {
        walkExpr(*branch.condition, table, diags);
        if (!isBooleanExpr(*branch.condition, table)) {
          diags.addError(Location{"", branch.condition->token.line, branch.condition->token.col},
                         Code::ExprIfConditionNotBoolean, "if 条件必须是布尔表达式");
        }
      }
      for (const auto &sub : branch.equations)
        walkEquationExpr(sub, table, diags);
    }
    return;
  }
  if (!eq.rhs.isDer && eq.rhs.expr)
    walkExpr(*eq.rhs.expr, table, diags);
  if (!eq.lhs.isDer && eq.lhs.expr)
    walkExpr(*eq.lhs.expr, table, diags);
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
    walkEquationExpr(eq, table, diags);
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

bool isBooleanExpr(const ast::Expr &expr, const SymbolTable &table) {
  switch (expr.kind) {
  case ast::ExprKind::BoolLit:
    return true;
  case ast::ExprKind::Ident: {
    const SymbolInfo *info = table.find(expr.token.lexeme);
    return info != nullptr && info->type == ast::Component::DeclType::Boolean;
  }
  case ast::ExprKind::Unary:
    return expr.op == "not";
  case ast::ExprKind::Binary: {
    static const std::unordered_set<std::string> kComparison = {
        "<", "<=", ">", ">=", "==", "<>", "and", "or",
    };
    return kComparison.count(expr.op) != 0;
  }
  case ast::ExprKind::If:
    return expr.thenExpr ? isBooleanExpr(*expr.thenExpr, table) : false;
  case ast::ExprKind::NumLit:
  case ast::ExprKind::Call:
  case ast::ExprKind::Der:
    return false;
  }
  return false;
}

ExprTypeClass typeClassOf(const ast::Expr &expr, const SymbolTable &table) {
  switch (expr.kind) {
  case ast::ExprKind::NumLit:
    return ExprTypeClass::Numeric;
  case ast::ExprKind::BoolLit:
    return ExprTypeClass::Boolean;
  case ast::ExprKind::Ident: {
    const SymbolInfo *info = table.find(expr.token.lexeme);
    if (info == nullptr)
      return ExprTypeClass::Unknown;
    return info->type == ast::Component::DeclType::Boolean ? ExprTypeClass::Boolean
                                                           : ExprTypeClass::Numeric;
  }
  case ast::ExprKind::Unary:
    return expr.op == "not" ? ExprTypeClass::Boolean : ExprTypeClass::Numeric;
  case ast::ExprKind::Binary: {
    static const std::unordered_set<std::string> kBooleanOps = {
        "<", "<=", ">", ">=", "==", "<>", "and", "or",
    };
    return kBooleanOps.count(expr.op) != 0 ? ExprTypeClass::Boolean : ExprTypeClass::Numeric;
  }
  case ast::ExprKind::Call:
    return ExprTypeClass::Numeric;
  case ast::ExprKind::If:
    return expr.thenExpr ? typeClassOf(*expr.thenExpr, table) : ExprTypeClass::Unknown;
  case ast::ExprKind::Der:
    break;
  }
  return ExprTypeClass::Unknown;
}

} // namespace mcdc
