#pragma once
#include <optional>
#include <set>
#include <string>

#include "ast/ast.h"

namespace mcdc::eqir {

void collectIdents(const ast::Expr &expr, std::set<std::string> *out);
ast::ExprPtr cloneExpr(const ast::Expr &e);

// 构造 residual: lhsIdent - rhs  （== 0）
ast::ExprPtr makeResidual(const Token &locTok, const std::string &lhsName, const ast::Expr &rhs);

// 将表达式中对 from 的 Ident 替换为 to 的克隆（scale≠1 时变为 scale*to）。
ast::ExprPtr substituteIdent(const ast::Expr &e, const std::string &from, const std::string &to,
                             double scale = 1.0);

// 将 Ident from 替换为数值字面量。
ast::ExprPtr substituteIdentWithNum(const ast::Expr &e, const std::string &from, double value);

std::optional<double> tryEvalConstSilent(const ast::Expr &expr, const ast::Model &model,
                                         std::set<std::string> *visiting);
std::optional<bool> tryEvalBoolCond(const ast::Expr &cond, const ast::Model &model);

} // namespace mcdc::eqir
