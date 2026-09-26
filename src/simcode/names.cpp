#include "simcode/names.h"

#include <cstdio>
#include <unordered_set>

#include "eqir/ir.h"

namespace mcdc {

namespace {

const std::unordered_set<std::string> &kCppKeywords() {
  static const std::unordered_set<std::string> kSet = {
      "alignas",      "alignof",   "and",           "and_eq",
      "asm",          "auto",      "bitand",        "bitor",
      "bool",         "break",     "case",          "catch",
      "char",         "char8_t",   "class",         "compl",
      "concept",      "const",     "consteval",     "constexpr",
      "constinit",    "const_cast","continue",      "co_await",
      "co_return",    "co_yield",  "decltype",      "default",
      "delete",       "do",        "double",        "dynamic_cast",
      "else",         "enum",      "explicit",      "export",
      "extern",       "false",     "float",         "for",
      "friend",       "goto",      "if",            "inline",
      "int",          "long",      "mutable",       "namespace",
      "new",          "noexcept",  "not",           "not_eq",
      "nullptr",      "operator",  "or",            "or_eq",
      "private",      "protected", "public",        "register",
      "reinterpret_cast", "requires", "return",     "short",
      "signed",       "sizeof",    "static",        "static_assert",
      "static_cast",  "struct",    "switch",        "template",
      "this",         "thread_local", "throw",      "true",
      "try",          "typedef",   "typeid",        "typename",
      "union",        "unsigned",  "using",         "virtual",
      "void",         "volatile",  "wchar_t",       "while",
      "xor",          "xor_eq",
  };
  return kSet;
}

} // namespace

std::string mapToCppIdentifier(const std::string &name) {
  if (kCppKeywords().count(name))
    return name + "_";
  return name;
}

std::string mapInitUnknownToCpp(const std::string &name) {
  if (eqir::isDerUnknownName(name))
    return "der_" + mapToCppIdentifier(eqir::derStateName(name));
  return mapToCppIdentifier(name);
}

std::string formatRealLiteral(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  return buf;
}

std::string declTypeName(ast::Component::DeclType type) {
  switch (type) {
  case ast::Component::DeclType::Real:
    return "double";
  case ast::Component::DeclType::Integer:
    return "long long";
  case ast::Component::DeclType::Boolean:
    return "bool";
  }
  return "double";
}

std::optional<double> evalConstExpr(const ast::Expr &expr, const ast::Model &model,
                                    const SymbolTable &table, std::set<std::string> *visiting,
                                    DiagnosticCollector &diags) {
  (void)table;
  switch (expr.kind) {
  case ast::ExprKind::NumLit:
    return expr.numValue;
  case ast::ExprKind::BoolLit:
    return expr.boolValue ? 1.0 : 0.0;
  case ast::ExprKind::Der:
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                   "初始化/属性值必须是数值常量表达式");
    return std::nullopt;
  case ast::ExprKind::Ident: {
    const std::string &name = expr.token.lexeme;
    if (visiting->count(name)) {
      diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                     "常量表达式存在循环引用 \"" + name + "\"");
      return std::nullopt;
    }
    for (const auto &comp : model.components) {
      if (comp.nameTok.lexeme != name)
        continue;
      if (comp.kind == ast::Component::Kind::Variable || !comp.value) {
        diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                       "常量表达式中引用了非常量 \"" + name + "\"");
        return std::nullopt;
      }
      visiting->insert(name);
      auto value = evalConstExpr(*comp.value, model, table, visiting, diags);
      visiting->erase(name);
      return value;
    }
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::SymbolUndeclared,
                   "使用未声明标识符 \"" + name + "\"");
    return std::nullopt;
  }
  case ast::ExprKind::Unary: {
    auto operand = evalConstExpr(*expr.lhs, model, table, visiting, diags);
    if (!operand)
      return std::nullopt;
    return -*operand;
  }
  case ast::ExprKind::Binary: {
    auto l = evalConstExpr(*expr.lhs, model, table, visiting, diags);
    if (!l)
      return std::nullopt;
    auto r = evalConstExpr(*expr.rhs, model, table, visiting, diags);
    if (!r)
      return std::nullopt;
    if (expr.op == "+")
      return *l + *r;
    if (expr.op == "-")
      return *l - *r;
    if (expr.op == "*")
      return *l * *r;
    if (expr.op == "/") {
      if (*r == 0.0) {
        diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                       "常量表达式除零");
        return std::nullopt;
      }
      return *l / *r;
    }
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                   "初始化值仅支持算术运算（+ - * /）");
    return std::nullopt;
  }
  case ast::ExprKind::Call:
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                   "初始化值不支持函数调用");
    return std::nullopt;
  case ast::ExprKind::If: {
    auto c = evalConstExpr(*expr.cond, model, table, visiting, diags);
    if (!c)
      return std::nullopt;
    const ast::Expr *pick = (*c != 0.0) ? expr.thenExpr.get() : expr.elseExpr.get();
    if (pick == nullptr)
      return std::nullopt;
    return evalConstExpr(*pick, model, table, visiting, diags);
  }
  }
  return std::nullopt;
}

} // namespace mcdc
