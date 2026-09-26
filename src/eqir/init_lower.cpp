#include "eqir/init_lower.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "eqir/expr_util.h"

namespace mcdc::eqir {
namespace {

struct GenDef {
  bool ok = true;
  bool isState = false;
  std::string target{};
  const ast::Expr *rhs = nullptr;
};

GenDef reduceEquation(const ast::Equation &eq, const ast::Model &model,
                      std::vector<ast::ExprPtr> *synthesized, DiagnosticCollector &diags, int depth);

GenDef reduceIfEquation(const ast::Equation &eq, const ast::Model &model,
                        std::vector<ast::ExprPtr> *synthesized, DiagnosticCollector &diags,
                        int depth) {
  constexpr int kMaxReduceDepth = 4096;
  if (depth >= kMaxReduceDepth) {
    diags.addError(Location{"", eq.ifTok.line, eq.ifTok.col}, Code::EqIfBranchMismatch,
                   "条件方程嵌套超过安全深度");
    return {.ok = false};
  }
  const size_t n = eq.branches.size();
  if (n == 0) {
    diags.addError(Location{"", eq.ifTok.line, eq.ifTok.col}, Code::EqIfBranchMismatch,
                   "条件方程缺少任何分支");
    return {.ok = false};
  }
  for (const auto &b : eq.branches) {
    if (b.equations.size() != 1) {
      diags.addError(Location{"", eq.ifTok.line, eq.ifTok.col}, Code::EqIfBranchMismatch,
                     "条件方程每个分支必须恰好包含一个方程");
      return {.ok = false};
    }
  }
  const bool hasElse = eq.branches.back().condition == nullptr;
  {
    size_t selected = n;
    bool runtime = false;
    for (size_t i = 0; i < n; ++i) {
      if (!eq.branches[i].condition) {
        selected = i;
        break;
      }
      auto c = tryEvalBoolCond(*eq.branches[i].condition, model);
      if (!c) {
        runtime = true;
        break;
      }
      if (*c) {
        selected = i;
        break;
      }
    }
    if (selected < n)
      return reduceEquation(eq.branches[selected].equations[0], model, synthesized, diags,
                            depth + 1);
    if (!hasElse) {
      diags.addError(Location{"", eq.ifTok.line, eq.ifTok.col}, Code::EqIfBranchMismatch,
                     runtime ? "条件方程缺少 else 分支，且条件不是编译期常量"
                             : "条件方程缺少 else 且所有条件恒假：未知量永不被定义");
      return {.ok = false};
    }
  }
  std::vector<GenDef> defs;
  for (const auto &b : eq.branches) {
    auto d = reduceEquation(b.equations[0], model, synthesized, diags, depth + 1);
    if (!d.ok)
      return d;
    defs.push_back(d);
  }
  for (size_t i = 1; i < defs.size(); ++i) {
    if (defs[i].target != defs[0].target || defs[i].isState != defs[0].isState) {
      diags.addError(Location{"", eq.ifTok.line, eq.ifTok.col}, Code::EqIfBranchMismatch,
                     "条件方程各分支必须求解同一未知量");
      return {.ok = false};
    }
  }
  ast::ExprPtr merged = cloneExpr(*defs.back().rhs);
  size_t i = defs.size() - 1;
  while (i > 0) {
    --i;
    merged = ast::Expr::if_(eq.ifTok, cloneExpr(*eq.branches[i].condition),
                            cloneExpr(*defs[i].rhs), std::move(merged));
  }
  synthesized->push_back(std::move(merged));
  GenDef g = defs[0];
  g.rhs = synthesized->back().get();
  return g;
}

GenDef reduceEquation(const ast::Equation &eq, const ast::Model &model,
                      std::vector<ast::ExprPtr> *synthesized, DiagnosticCollector &diags,
                      int depth) {
  if (eq.isIf)
    return reduceIfEquation(eq, model, synthesized, diags, depth);
  // 初始系统：两侧 der 均合法，映射到 der(name) 未知量。
  if (eq.lhs.isDer) {
    return {.ok = true, .isState = true, .target = derUnknownName(eq.lhs.targetTok.lexeme),
            .rhs = eq.rhs.expr.get()};
  }
  if (eq.rhs.isDer) {
    // x = der(y) → 写成 der(y) 在右侧；转为 preferred=x, rhs=Ident der(y) 不便。
    // 规范为 residual：构造 Ident der(y) 作为 rhs。
    return {.ok = false}; // 下面单独处理
  }
  if (!eq.lhs.expr || eq.lhs.expr->kind != ast::ExprKind::Ident) {
    diags.addError(Location{"", eq.lhs.expr ? eq.lhs.expr->token.line : 0,
                            eq.lhs.expr ? eq.lhs.expr->token.col : 0},
                   Code::ExprUnsupported, "方程左侧必须是变量或 der(变量)");
    return {.ok = false};
  }
  return {.ok = true, .isState = false, .target = eq.lhs.expr->token.lexeme,
          .rhs = eq.rhs.expr.get()};
}

ast::ExprPtr makeDerIdent(const Token &loc, const std::string &state) {
  Token t = loc;
  t.kind = TokKind::Ident;
  t.lexeme = derUnknownName(state);
  return ast::Expr::ident(t);
}

bool fixedTrue(const ast::Component &comp) {
  return comp.hasFixed && comp.fixed && comp.fixed->kind == ast::ExprKind::BoolLit &&
         comp.fixed->boolValue;
}

} // namespace

std::optional<EqModule> lowerInitial(const ast::Model &model, const SymbolTable &table,
                                     const EqModule &continuous, DiagnosticCollector &diags) {
  EqModule mod;
  mod.modelName = model.nameTok.lexeme;
  mod.modelTok = model.modelTok;
  mod.nameTok = model.nameTok;
  mod.isInitial = true;
  mod.startTime = continuous.startTime;
  mod.stopTime = continuous.stopTime;
  mod.interval = continuous.interval;
  mod.experimentOk = continuous.experimentOk;
  mod.states = continuous.states;

  std::set<std::string> stateSet(continuous.states.begin(), continuous.states.end());

  size_t declOrder = 0;
  for (const auto &sym : table.all()) {
    if (sym.kind != ast::Component::Kind::Variable)
      continue;
    Unknown u;
    u.name = sym.name;
    u.role = UnknownRole::Algebraic;
    u.type = sym.type;
    u.declOrder = declOrder++;
    mod.unknowns.push_back(u);

    double guess = 0.0;
    for (const auto &comp : model.components) {
      if (comp.nameTok.lexeme != sym.name)
        continue;
      if (comp.start) {
        std::set<std::string> visiting;
        auto v = tryEvalConstSilent(*comp.start, model, &visiting);
        if (v)
          guess = *v;
      }
      break;
    }
    mod.guesses[sym.name] = guess;

    if (stateSet.count(sym.name)) {
      Unknown d;
      d.name = derUnknownName(sym.name);
      d.role = UnknownRole::Algebraic;
      d.type = ast::Component::DeclType::Real;
      d.declOrder = declOrder++;
      mod.unknowns.push_back(d);
      mod.guesses[d.name] = 0.0;
    }
  }

  std::vector<ast::ExprPtr> synthesized;
  size_t nextId = 0;
  bool ok = true;

  auto pushEq = [&](const std::string &preferred, const ast::Expr *rhs, Token loc) {
    if (rhs == nullptr) {
      ok = false;
      return;
    }
    Equation eq;
    eq.id = nextId++;
    eq.isStateDeriv = false;
    eq.preferredUnknown = preferred;
    eq.rhs = cloneExpr(*rhs);
    eq.locTok = loc;
    mod.equations.push_back(std::move(eq));
  };

  auto pushEqOwned = [&](const std::string &preferred, ast::ExprPtr rhs, Token loc) {
    Equation eq;
    eq.id = nextId++;
    eq.isStateDeriv = false;
    eq.preferredUnknown = preferred;
    eq.rhs = std::move(rhs);
    eq.locTok = loc;
    mod.equations.push_back(std::move(eq));
  };

  // 连续方程 → 初始系统
  for (const auto &eq : continuous.equations) {
    if (eq.isStateDeriv) {
      pushEq(derUnknownName(eq.preferredUnknown), eq.rhs.get(), eq.locTok);
    } else {
      pushEq(eq.preferredUnknown, eq.rhs.get(), eq.locTok);
    }
  }

  // fixed=true → x = start
  for (const auto &comp : model.components) {
    if (comp.kind != ast::Component::Kind::Variable)
      continue;
    if (!fixedTrue(comp))
      continue;
    Token loc = comp.nameTok;
    if (comp.start) {
      pushEq(comp.nameTok.lexeme, comp.start.get(), loc);
    } else {
      Token numTok = loc;
      numTok.kind = TokKind::Real;
      numTok.lexeme = "0";
      pushEqOwned(comp.nameTok.lexeme, ast::Expr::num(numTok, 0.0), loc);
    }
  }

  // initial equation 段
  for (const auto &eq : model.initialEquations) {
    if (eq.isIf) {
      GenDef def = reduceEquation(eq, model, &synthesized, diags, 0);
      if (!def.ok) {
        ok = false;
        continue;
      }
      // isState 在 reduce 里表示 LHS 为 der → target 已是 der(name)
      pushEq(def.target, def.rhs, eq.ifTok);
      continue;
    }
    if (eq.lhs.isDer) {
      pushEq(derUnknownName(eq.lhs.targetTok.lexeme), eq.rhs.expr.get(), eq.lhs.derToken);
      continue;
    }
    if (eq.rhs.isDer) {
      // x = der(y)
      if (!eq.lhs.expr || eq.lhs.expr->kind != ast::ExprKind::Ident) {
        diags.addError(Location{"", eq.rhs.derToken.line, eq.rhs.derToken.col},
                       Code::ExprUnsupported, "初始方程右侧 der 时左侧须为变量");
        ok = false;
        continue;
      }
      auto derId = makeDerIdent(eq.rhs.derToken, eq.rhs.targetTok.lexeme);
      pushEqOwned(eq.lhs.expr->token.lexeme, std::move(derId), eq.lhs.expr->token);
      continue;
    }
    GenDef def = reduceEquation(eq, model, &synthesized, diags, 0);
    if (!def.ok) {
      ok = false;
      continue;
    }
    pushEq(def.target, def.rhs, eq.lhs.expr ? eq.lhs.expr->token : eq.lhs.derToken);
  }

  for (auto &s : synthesized)
    mod.extras.push_back(std::move(s));

  const size_t nU = mod.unknowns.size();
  const size_t nE = mod.equations.size();
  if (nE < nU) {
    diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::InitUnderdetermined,
                   "初始系统欠定：方程数(" + std::to_string(nE) + ") 少于未知量数(" +
                       std::to_string(nU) +
                       ")（可为状态加 fixed=true 或补充 initial equation）");
    ok = false;
  } else if (nE > nU) {
    diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::InitOverdetermined,
                   "初始系统超定：方程数(" + std::to_string(nE) + ") 多于未知量数(" +
                       std::to_string(nU) + ")");
    ok = false;
  }

  if (!ok)
    return std::nullopt;
  return mod;
}

} // namespace mcdc::eqir
