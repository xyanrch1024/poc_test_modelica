#include "eqir/lower.h"

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
      if (runtime) {
        diags.addError(Location{"", eq.ifTok.line, eq.ifTok.col}, Code::EqIfBranchMismatch,
                       "条件方程缺少 else 分支，且条件不是编译期常量");
      } else {
        diags.addError(Location{"", eq.ifTok.line, eq.ifTok.col}, Code::EqIfBranchMismatch,
                       "条件方程缺少 else 且所有条件恒假：未知量永不被定义");
      }
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
                     "条件方程各分支必须求解同一未知量（同为变量或同为 der(变量)）");
      return {.ok = false};
    }
  }

  ast::ExprPtr merged = cloneExpr(*defs.back().rhs);
  size_t i = defs.size() - 1;
  while (i > 0) {
    --i;
    ast::ExprPtr cond = cloneExpr(*eq.branches[i].condition);
    ast::ExprPtr val = cloneExpr(*defs[i].rhs);
    merged = ast::Expr::if_(eq.ifTok, std::move(cond), std::move(val), std::move(merged));
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

  if (eq.rhs.isDer) {
    diags.addError(Location{"", eq.rhs.derToken.line, eq.rhs.derToken.col},
                   Code::ExprBadDerPlacement, "der(...) 只能出现在方程左侧");
    return {.ok = false};
  }
  if (eq.lhs.isDer) {
    return {.ok = true, .isState = true, .target = eq.lhs.targetTok.lexeme,
            .rhs = eq.rhs.expr.get()};
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

} // namespace

std::optional<EqModule> lower(const ast::Model &model, const SymbolTable &table,
                              DiagnosticCollector &diags) {
  EqModule mod;
  mod.modelName = model.nameTok.lexeme;
  mod.modelTok = model.modelTok;
  mod.nameTok = model.nameTok;

  size_t declOrder = 0;
  for (const auto &sym : table.all()) {
    if (sym.kind != ast::Component::Kind::Variable)
      continue;
    Unknown u;
    u.name = sym.name;
    u.role = UnknownRole::Algebraic; // 稍后标记 State
    u.type = sym.type;
    u.declOrder = declOrder++;
    mod.unknowns.push_back(std::move(u));
  }

  std::vector<ast::ExprPtr> synthesized;
  std::map<std::string, GenDef> defs; // target → def
  bool ok = true;
  size_t nextId = 0;

  auto addDef = [&](const GenDef &def, const Token &locTok) {
    if (!def.ok) {
      ok = false;
      return;
    }
    if (defs.count(def.target)) {
      diags.addError(Location{"", locTok.line, locTok.col}, Code::EqOverdetermined,
                     "变量 \"" + def.target + "\" 被多个方程重复定义");
      ok = false;
      return;
    }
    if (def.isState) {
      const SymbolInfo *info = table.find(def.target);
      if (info == nullptr) {
        ok = false;
        return;
      }
      if (info->kind != ast::Component::Kind::Variable ||
          info->type != ast::Component::DeclType::Real) {
        diags.addError(Location{"", locTok.line, locTok.col}, Code::ExprBadDerPlacement,
                       "求导目标必须是 Real 类型的普通变量 \"" + def.target + "\"");
        ok = false;
        return;
      }
      if (auto *u = findUnknownMutable(mod, def.target))
        u->role = UnknownRole::State;
    } else {
      const SymbolInfo *info = table.find(def.target);
      if (info != nullptr && info->kind != ast::Component::Kind::Variable) {
        diags.addError(Location{"", locTok.line, locTok.col}, Code::EqOverdetermined,
                       "不得对参数/常量 \"" + def.target + "\" 赋值（模型超定）");
        ok = false;
        return;
      }
    }
    defs[def.target] = def;

    Equation eq;
    eq.id = nextId++;
    eq.isStateDeriv = def.isState;
    eq.preferredUnknown = def.target;
    eq.rhs = cloneExpr(*def.rhs);
    eq.locTok = locTok;
    mod.equations.push_back(std::move(eq));
  };

  for (const auto &eq : model.equations) {
    if (eq.isIf) {
      GenDef def = reduceEquation(eq, model, &synthesized, diags, 0);
      addDef(def, eq.ifTok);
      continue;
    }
    if (eq.rhs.isDer) {
      diags.addError(Location{"", eq.rhs.derToken.line, eq.rhs.derToken.col},
                     Code::ExprBadDerPlacement, "der(...) 只能出现在方程左侧");
      ok = false;
      continue;
    }
    GenDef def = reduceEquation(eq, model, &synthesized, diags, 0);
    Token loc = eq.lhs.isDer ? eq.lhs.derToken
                             : (eq.lhs.expr ? eq.lhs.expr->token : eq.lhs.derToken);
    addDef(def, loc);
  }

  // 归约产物转入 extras（方程已持有克隆）。
  for (auto &s : synthesized)
    mod.extras.push_back(std::move(s));

  // 配平：未知量数 == 方程数
  const size_t nUnknown = mod.unknowns.size();
  const size_t nEq = mod.equations.size();
  if (nEq < nUnknown) {
    diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::EqUnderdetermined,
                   "欠定模型：方程数(" + std::to_string(nEq) + ") 少于未知量数(" +
                       std::to_string(nUnknown) + ")");
    ok = false;
  } else if (nEq > nUnknown) {
    diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::EqOverdetermined,
                   "超定模型：方程数(" + std::to_string(nEq) + ") 多于未知量数(" +
                       std::to_string(nUnknown) + ")");
    ok = false;
  }

  // 每个未知量必须有定义方程
  for (const auto &u : mod.unknowns) {
    if (!defs.count(u.name)) {
      // 已由配平覆盖；保持与旧行为一致的欠定报告即可
    }
  }

  // 状态初始化 MC0304
  std::set<std::string> stateNames;
  for (const auto &u : mod.unknowns) {
    if (u.role == UnknownRole::State)
      stateNames.insert(u.name);
  }
  for (const auto &comp : model.components) {
    if (comp.kind != ast::Component::Kind::Variable)
      continue;
    if (!stateNames.count(comp.nameTok.lexeme))
      continue;
    const bool fixedTrue = comp.hasFixed && comp.fixed &&
                           comp.fixed->kind == ast::ExprKind::BoolLit && comp.fixed->boolValue;
    if (!comp.start && !fixedTrue) {
      diags.addError(Location{"", comp.nameTok.line, comp.nameTok.col}, Code::InitMissingStart,
                     "状态量 \"" + comp.nameTok.lexeme + "\" 缺少 start 初值且未声明 fixed=true");
      ok = false;
    }
  }

  // 实验注释 MC0401
  if (!model.experiment) {
    diags.addError(Location{"", model.nameTok.line, model.nameTok.col}, Code::ExperimentInvalid,
                   "缺少实验设置注释 annotation(experiment(StartTime=…, StopTime=…, Interval=…))");
    ok = false;
  } else {
    mod.startTime = model.experiment->startTime;
    mod.stopTime = model.experiment->stopTime;
    mod.interval = model.experiment->interval;
    if (!model.experiment->hasStopTime || !model.experiment->hasInterval ||
        !(mod.stopTime > mod.startTime) || !(mod.interval > 0.0) ||
        mod.interval > (mod.stopTime - mod.startTime) + 1e-12) {
      diags.addError(Location{"", model.experiment->annTok.line, model.experiment->annTok.col},
                     Code::ExperimentInvalid,
                     "实验设置无效：需 StopTime > StartTime 且 0 < Interval ≤ 时间跨度");
      ok = false;
    } else {
      mod.experimentOk = true;
    }
  }

  if (!ok)
    return std::nullopt;

  for (const auto &u : mod.unknowns) {
    if (u.role == UnknownRole::State)
      mod.states.push_back(u.name);
  }
  for (const auto &eq : mod.equations) {
    if (eq.isStateDeriv)
      mod.stateRhs[eq.preferredUnknown] = eq.rhs.get();
  }
  return mod;
}

} // namespace mcdc::eqir
