#include "eqir/expr_util.h"

#include <set>

namespace mcdc::eqir {

void collectIdents(const ast::Expr &expr, std::set<std::string> *out) {
  switch (expr.kind) {
  case ast::ExprKind::Ident:
    out->insert(expr.token.lexeme);
    break;
  case ast::ExprKind::NumLit:
  case ast::ExprKind::BoolLit:
    break;
  case ast::ExprKind::Unary:
    if (expr.lhs)
      collectIdents(*expr.lhs, out);
    break;
  case ast::ExprKind::Binary:
    if (expr.lhs)
      collectIdents(*expr.lhs, out);
    if (expr.rhs)
      collectIdents(*expr.rhs, out);
    break;
  case ast::ExprKind::Call:
    for (const auto &arg : expr.args) {
      if (arg)
        collectIdents(*arg, out);
    }
    break;
  case ast::ExprKind::Der:
    if (expr.lhs)
      collectIdents(*expr.lhs, out);
    break;
  case ast::ExprKind::If:
    if (expr.cond)
      collectIdents(*expr.cond, out);
    if (expr.thenExpr)
      collectIdents(*expr.thenExpr, out);
    if (expr.elseExpr)
      collectIdents(*expr.elseExpr, out);
    break;
  }
}

ast::ExprPtr cloneExpr(const ast::Expr &e) {
  auto c = std::make_unique<ast::Expr>();
  c->kind = e.kind;
  c->token = e.token;
  c->numValue = e.numValue;
  c->boolValue = e.boolValue;
  c->op = e.op;
  if (e.lhs)
    c->lhs = cloneExpr(*e.lhs);
  if (e.rhs)
    c->rhs = cloneExpr(*e.rhs);
  if (e.cond)
    c->cond = cloneExpr(*e.cond);
  if (e.thenExpr)
    c->thenExpr = cloneExpr(*e.thenExpr);
  if (e.elseExpr)
    c->elseExpr = cloneExpr(*e.elseExpr);
  for (const auto &a : e.args)
    c->args.push_back(a ? cloneExpr(*a) : nullptr);
  return c;
}

ast::ExprPtr makeResidual(const Token &locTok, const std::string &lhsName, const ast::Expr &rhs) {
  Token idTok = locTok;
  idTok.kind = TokKind::Ident;
  idTok.lexeme = lhsName;
  auto lhs = ast::Expr::ident(idTok);
  auto rhsClone = cloneExpr(rhs);
  return ast::Expr::binary(locTok, "-", std::move(lhs), std::move(rhsClone));
}

ast::ExprPtr substituteIdent(const ast::Expr &e, const std::string &from, const std::string &to,
                             double scale) {
  if (e.kind == ast::ExprKind::Ident && e.token.lexeme == from) {
    Token t = e.token;
    t.lexeme = to;
    auto id = ast::Expr::ident(t);
    if (scale == 1.0)
      return id;
    if (scale == -1.0)
      return ast::Expr::unary(e.token, "-", std::move(id));
    Token numTok = e.token;
    numTok.kind = TokKind::Real;
    numTok.lexeme = "scale";
    auto num = ast::Expr::num(numTok, scale);
    return ast::Expr::binary(e.token, "*", std::move(num), std::move(id));
  }
  auto c = std::make_unique<ast::Expr>();
  c->kind = e.kind;
  c->token = e.token;
  c->numValue = e.numValue;
  c->boolValue = e.boolValue;
  c->op = e.op;
  if (e.lhs)
    c->lhs = substituteIdent(*e.lhs, from, to, scale);
  if (e.rhs)
    c->rhs = substituteIdent(*e.rhs, from, to, scale);
  if (e.cond)
    c->cond = substituteIdent(*e.cond, from, to, scale);
  if (e.thenExpr)
    c->thenExpr = substituteIdent(*e.thenExpr, from, to, scale);
  if (e.elseExpr)
    c->elseExpr = substituteIdent(*e.elseExpr, from, to, scale);
  for (const auto &a : e.args)
    c->args.push_back(a ? substituteIdent(*a, from, to, scale) : nullptr);
  return c;
}

ast::ExprPtr substituteIdentWithNum(const ast::Expr &e, const std::string &from, double value) {
  if (e.kind == ast::ExprKind::Ident && e.token.lexeme == from) {
    Token t = e.token;
    t.kind = TokKind::Real;
    return ast::Expr::num(t, value);
  }
  auto c = std::make_unique<ast::Expr>();
  c->kind = e.kind;
  c->token = e.token;
  c->numValue = e.numValue;
  c->boolValue = e.boolValue;
  c->op = e.op;
  if (e.lhs)
    c->lhs = substituteIdentWithNum(*e.lhs, from, value);
  if (e.rhs)
    c->rhs = substituteIdentWithNum(*e.rhs, from, value);
  if (e.cond)
    c->cond = substituteIdentWithNum(*e.cond, from, value);
  if (e.thenExpr)
    c->thenExpr = substituteIdentWithNum(*e.thenExpr, from, value);
  if (e.elseExpr)
    c->elseExpr = substituteIdentWithNum(*e.elseExpr, from, value);
  for (const auto &a : e.args)
    c->args.push_back(a ? substituteIdentWithNum(*a, from, value) : nullptr);
  return c;
}

std::optional<double> tryEvalConstSilent(const ast::Expr &expr, const ast::Model &model,
                                         std::set<std::string> *visiting) {
  switch (expr.kind) {
  case ast::ExprKind::NumLit:
    return expr.numValue;
  case ast::ExprKind::BoolLit:
    return expr.boolValue ? 1.0 : 0.0;
  case ast::ExprKind::Ident: {
    const std::string &name = expr.token.lexeme;
    if (visiting->count(name))
      return std::nullopt;
    for (const auto &comp : model.components) {
      if (comp.nameTok.lexeme != name)
        continue;
      if (comp.kind != ast::Component::Kind::Constant &&
          comp.kind != ast::Component::Kind::Parameter)
        return std::nullopt;
      if (!comp.value)
        return std::nullopt;
      visiting->insert(name);
      auto v = tryEvalConstSilent(*comp.value, model, visiting);
      visiting->erase(name);
      return v;
    }
    return std::nullopt;
  }
  case ast::ExprKind::Unary: {
    auto o = tryEvalConstSilent(*expr.lhs, model, visiting);
    if (!o)
      return std::nullopt;
    return expr.op == "not" ? (*o == 0.0 ? 1.0 : 0.0) : (expr.op == "-" ? -*o : *o);
  }
  case ast::ExprKind::Binary: {
    auto l = tryEvalConstSilent(*expr.lhs, model, visiting);
    if (!l)
      return std::nullopt;
    auto r = tryEvalConstSilent(*expr.rhs, model, visiting);
    if (!r)
      return std::nullopt;
    const double a = *l, b = *r;
    if (expr.op == "+")
      return a + b;
    if (expr.op == "-")
      return a - b;
    if (expr.op == "*")
      return a * b;
    if (expr.op == "/")
      return b == 0.0 ? std::nullopt : std::make_optional(a / b);
    if (expr.op == "<")
      return a < b ? 1.0 : 0.0;
    if (expr.op == "<=")
      return a <= b ? 1.0 : 0.0;
    if (expr.op == ">")
      return a > b ? 1.0 : 0.0;
    if (expr.op == ">=")
      return a >= b ? 1.0 : 0.0;
    if (expr.op == "==")
      return a == b ? 1.0 : 0.0;
    if (expr.op == "<>")
      return a != b ? 1.0 : 0.0;
    if (expr.op == "and")
      return (a != 0.0 && b != 0.0) ? 1.0 : 0.0;
    if (expr.op == "or")
      return (a != 0.0 || b != 0.0) ? 1.0 : 0.0;
    return std::nullopt;
  }
  case ast::ExprKind::Call:
  case ast::ExprKind::Der:
  case ast::ExprKind::If:
    return std::nullopt;
  }
  return std::nullopt;
}

std::optional<bool> tryEvalBoolCond(const ast::Expr &cond, const ast::Model &model) {
  std::set<std::string> visiting;
  auto v = tryEvalConstSilent(cond, model, &visiting);
  if (!v)
    return std::nullopt;
  return *v != 0.0;
}

} // namespace mcdc::eqir
