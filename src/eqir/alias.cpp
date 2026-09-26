#include "eqir/alias.h"

#include "eqir/expr_util.h"

namespace mcdc::eqir {
namespace {

bool isAliasRhs(const ast::Expr &rhs, std::string *other, double *scale, bool *isConst,
                double *constVal) {
  *isConst = false;
  *scale = 1.0;
  if (rhs.kind == ast::ExprKind::NumLit) {
    *isConst = true;
    *constVal = rhs.numValue;
    return true;
  }
  if (rhs.kind == ast::ExprKind::BoolLit) {
    *isConst = true;
    *constVal = rhs.boolValue ? 1.0 : 0.0;
    return true;
  }
  if (rhs.kind == ast::ExprKind::Ident) {
    *other = rhs.token.lexeme;
    *scale = 1.0;
    return true;
  }
  if (rhs.kind == ast::ExprKind::Unary && rhs.op == "-" && rhs.lhs &&
      rhs.lhs->kind == ast::ExprKind::Ident) {
    *other = rhs.lhs->token.lexeme;
    *scale = -1.0;
    return true;
  }
  return false;
}

void rewriteRefs(EqModule &mod, const std::string &from, const AliasInfo &info) {
  for (auto &eq : mod.equations) {
    if (!eq.rhs)
      continue;
    if (info.isConst)
      eq.rhs = substituteIdentWithNum(*eq.rhs, from, info.constValue);
    else
      eq.rhs = substituteIdent(*eq.rhs, from, info.canonical, info.scale);
  }
}

} // namespace

bool eliminateAliases(EqModule &mod, DiagnosticCollector &diags) {
  (void)diags;
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < mod.equations.size(); ++i) {
      Equation &eq = mod.equations[i];
      if (eq.isStateDeriv || !eq.matched || !eq.rhs)
        continue;
      const std::string lhs = *eq.matched;
      if (mod.aliases.count(lhs))
        continue;

      std::string other;
      double scale = 1.0;
      bool isConst = false;
      double constVal = 0.0;
      if (!isAliasRhs(*eq.rhs, &other, &scale, &isConst, &constVal))
        continue;
      if (!isConst && other == lhs)
        continue;

      if (isConst)
        continue; // 常量赋值保留为 Assign，便于调度/测试可见

      // 跟随已有别名链
      std::string canon = other;
      double combined = scale;
      int guard = 0;
      while (mod.aliases.count(canon) && guard++ < 64) {
        const AliasInfo &ai = mod.aliases.at(canon);
        if (ai.isConst)
          break;
        combined *= ai.scale;
        canon = ai.canonical;
      }
      if (mod.aliases.count(canon) && mod.aliases.at(canon).isConst)
        continue;

      const Unknown *ul = findUnknown(mod, lhs);
      const Unknown *uc = findUnknown(mod, canon);
      if (uc == nullptr || uc->role != UnknownRole::Algebraic)
        continue;
      if (ul && uc && ul->declOrder <= uc->declOrder)
        continue;
      if (combined != 1.0 && combined != -1.0)
        continue;

      AliasInfo info;
      info.canonical = canon;
      info.scale = combined;

      mod.aliases[lhs] = info;
      rewriteRefs(mod, lhs, info);
      mod.equations.erase(mod.equations.begin() + static_cast<std::ptrdiff_t>(i));
      changed = true;
      break;
    }
  }

  mod.stateRhs.clear();
  for (auto &eq : mod.equations) {
    if (eq.isStateDeriv) {
      mod.stateRhs[eq.preferredUnknown] = eq.rhs.get();
    } else if (!eq.matched) {
      eq.matched = eq.preferredUnknown;
    }
  }
  return true;
}

} // namespace mcdc::eqir
