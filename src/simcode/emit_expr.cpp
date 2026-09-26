#include "simcode/emit_expr.h"

#include <stdexcept>
#include <unordered_map>

#include "simcode/names.h"

namespace mcdc::simcode {

std::string emitExpr(const ast::Expr &e, const RefMap &refs) {
  switch (e.kind) {
  case ast::ExprKind::NumLit:
    if (e.token.kind == TokKind::Int)
      return e.token.lexeme;
    return formatRealLiteral(e.numValue);
  case ast::ExprKind::BoolLit:
    return e.boolValue ? "true" : "false";
  case ast::ExprKind::Ident: {
    auto it = refs.find(e.token.lexeme);
    if (it == refs.end())
      throw std::logic_error("未映射的标识符: " + e.token.lexeme);
    return it->second;
  }
  case ast::ExprKind::Unary: {
    const std::string operand = emitExpr(*e.lhs, refs);
    return e.op == "-" ? "(-" + operand + ")" : "(!" + operand + ")";
  }
  case ast::ExprKind::Binary: {
    const std::string l = emitExpr(*e.lhs, refs);
    const std::string r = emitExpr(*e.rhs, refs);
    std::string op = e.op;
    if (op == "and")
      op = " && ";
    else if (op == "or")
      op = " || ";
    else if (op == "<>")
      op = " != ";
    else
      op = " " + op + " ";
    return "(" + l + op + r + ")";
  }
  case ast::ExprKind::Call: {
    static const std::unordered_map<std::string, std::string> kFnMap = {
        {"abs", "std::fabs"}, {"sqrt", "std::sqrt"}, {"sin", "std::sin"}, {"cos", "std::cos"},
        {"tan", "std::tan"},  {"exp", "std::exp"},   {"log", "std::log"}, {"log10", "std::log10"},
    };
    std::string fn;
    if (e.op == "min" || e.op == "max") {
      fn = e.op == "min" ? "std::min<double>" : "std::max<double>";
    } else {
      fn = kFnMap.at(e.op);
    }
    std::string out = fn + "(";
    for (size_t i = 0; i < e.args.size(); ++i) {
      if (i > 0)
        out += ", ";
      out += emitExpr(*e.args[i], refs);
    }
    out += ")";
    return out;
  }
  case ast::ExprKind::Der:
    throw std::logic_error("der(...) 不应残留于表达式树");
  case ast::ExprKind::If:
    return "(" + emitExpr(*e.cond, refs) + " ? " + emitExpr(*e.thenExpr, refs) + " : " +
           emitExpr(*e.elseExpr, refs) + ")";
  }
  throw std::logic_error("未知表达式类型");
}

} // namespace mcdc::simcode
