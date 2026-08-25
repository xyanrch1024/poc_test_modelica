#include "ast/ast.h"

namespace mcdc::ast {

ExprPtr Expr::num(Token tok, double value) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::NumLit;
  e->token = std::move(tok);
  e->numValue = value;
  return e;
}

ExprPtr Expr::boolean(Token tok, bool value) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::BoolLit;
  e->token = std::move(tok);
  e->boolValue = value;
  return e;
}

ExprPtr Expr::ident(Token tok) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::Ident;
  e->token = std::move(tok);
  return e;
}

ExprPtr Expr::unary(Token opTok, std::string op, ExprPtr operand) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::Unary;
  e->token = std::move(opTok);
  e->op = std::move(op);
  e->lhs = std::move(operand);
  return e;
}

ExprPtr Expr::binary(Token opTok, std::string op, ExprPtr l, ExprPtr r) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::Binary;
  e->token = std::move(opTok);
  e->op = std::move(op);
  e->lhs = std::move(l);
  e->rhs = std::move(r);
  return e;
}

ExprPtr Expr::call(Token nameTok, std::vector<ExprPtr> arguments) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::Call;
  e->token = std::move(nameTok);
  e->op = e->token.lexeme;
  e->args = std::move(arguments);
  return e;
}

ExprPtr Expr::der(Token derTok, Token targetTok) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::Der;
  e->token = std::move(derTok);
  // 目标变量名保存在 op 中以便快速访问；位置保留在 token 中。
  e->lhs = ident(std::move(targetTok));
  return e;
}

} // namespace mcdc::ast
