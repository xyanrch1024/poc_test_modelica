#include "semantic/equations.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace mcdc {

namespace {

// 收集表达式中引用的标识符（不深入 Der 的目标——Der 在表达式内已被判定非法）。
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
  }
}

// Tarjan SCC：返回是否存在代数环；存在时通过 members 输出环成员。
bool hasAlgebraicLoop(const std::vector<std::string> &nodes,
                      const std::unordered_map<std::string, std::set<std::string>> &deps,
                      std::vector<std::string> *members) {
  std::unordered_map<std::string, int> index, lowlink;
  std::unordered_map<std::string, bool> onStack;
  std::vector<std::string> stack;
  int counter = 0;
  bool loop = false;

  // 迭代式 Tarjan，避免深递归。
  std::function<void(const std::string &)> strongconnect = [&](const std::string &v) {
    index[v] = lowlink[v] = counter++;
    stack.push_back(v);
    onStack[v] = true;

    auto it = deps.find(v);
    if (it != deps.end()) {
      for (const std::string &w : it->second) {
        if (index.find(w) == index.end()) {
          strongconnect(w);
          lowlink[v] = std::min(lowlink[v], lowlink[w]);
        } else if (onStack[w]) {
          lowlink[v] = std::min(lowlink[v], index[w]);
        }
      }
    }

    if (lowlink[v] == index[v]) {
      std::vector<std::string> scc;
      while (!stack.empty()) {
        std::string w = stack.back();
        stack.pop_back();
        onStack[w] = false;
        scc.push_back(w);
        if (w == v)
          break;
      }
      bool selfLoop = scc.size() == 1 && deps.count(scc[0]) && deps.at(scc[0]).count(scc[0]);
      if (scc.size() > 1 || selfLoop) {
        if (!loop)
          *members = scc; // 只报告第一个发现的环
        loop = true;
      }
    }
  };

  for (const auto &node : nodes) {
    if (index.find(node) == index.end())
      strongconnect(node);
  }
  return loop;
}

} // namespace

std::optional<EquationAnalysis> analyzeEquations(const ast::Model &model, const SymbolTable &table,
                                                 DiagnosticCollector &diags) {
  bool ok = true;

  // ---- 分类未知量与定义 ----
  std::set<std::string> statesSet;
  std::map<std::string, const ast::Expr *> stateDefMap;
  std::map<std::string, const ast::Expr *> algDefMap;

  for (const auto &eq : model.equations) {
    // rhs 侧顶层 der → MC0203
    if (eq.rhs.isDer) {
      diags.addError(Location{"", eq.rhs.derToken.line, eq.rhs.derToken.col},
                     Code::ExprBadDerPlacement, "der(...) 只能出现在方程左侧");
      ok = false;
      continue;
    }

    if (eq.lhs.isDer) {
      const std::string &name = eq.lhs.targetTok.lexeme;
      const SymbolInfo *info = table.find(name);
      if (info == nullptr)
        continue; // 未声明错误已由表达式分析报告
      if (info->kind != ast::Component::Kind::Variable ||
          info->type != ast::Component::DeclType::Real) {
        diags.addError(Location{"", eq.lhs.targetTok.line, eq.lhs.targetTok.col},
                       Code::ExprBadDerPlacement,
                       "求导目标必须是 Real 类型的普通变量 \"" + name + "\"");
        ok = false;
        continue;
      }
      if (statesSet.count(name) || stateDefMap.count(name)) {
        diags.addError(Location{"", eq.lhs.derToken.line, eq.lhs.derToken.col},
                       Code::EqOverdetermined, "变量 \"" + name + "\" 被多个方程重复定义");
        ok = false;
        continue;
      }
      statesSet.insert(name);
      stateDefMap[name] = eq.rhs.expr.get();
      continue;
    }

    // 普通方程：lhs 必须恰好是单个变量标识符。
    if (!eq.lhs.expr || eq.lhs.expr->kind != ast::ExprKind::Ident) {
      diags.addError(Location{"", eq.lhs.expr ? eq.lhs.expr->token.line : 0,
                              eq.lhs.expr ? eq.lhs.expr->token.col : 0},
                     Code::ExprUnsupported, "方程左侧必须是变量或 der(变量)");
      ok = false;
      continue;
    }
    const std::string &lhsName = eq.lhs.expr->token.lexeme;
    if (stateDefMap.count(lhsName)) {
      diags.addError(Location{"", eq.lhs.expr->token.line, eq.lhs.expr->token.col},
                     Code::EqOverdetermined, "变量 \"" + lhsName + "\" 已有导数定义，不得再次赋值");
      ok = false;
      continue;
    }
    if (algDefMap.count(lhsName)) {
      diags.addError(Location{"", eq.lhs.expr->token.line, eq.lhs.expr->token.col},
                     Code::EqOverdetermined, "变量 \"" + lhsName + "\" 被多个方程重复定义");
      ok = false;
      continue;
    }
    algDefMap[lhsName] = eq.rhs.expr.get();
  }

  // ---- 配平检查 ----
  std::vector<std::string> unknowns; // 声明序
  for (const auto &sym : table.all()) {
    if (sym.kind == ast::Component::Kind::Variable)
      unknowns.push_back(sym.name);
  }
  const size_t needed = unknowns.size();
  const size_t defined = stateDefMap.size() + algDefMap.size();

  if (defined < needed) {
    diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::EqUnderdetermined,
                   "欠定模型：方程数(" + std::to_string(defined) + ") 少于未知量数(" +
                       std::to_string(needed) + ")");
    ok = false;
  } else if (defined > needed) {
    diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::EqOverdetermined,
                   "超定模型：方程数(" + std::to_string(defined) + ") 多于未知量数(" +
                       std::to_string(needed) + ")");
    ok = false;
  }

  // 每个未知量必须有定义；对非未知量的赋值视为超定。
  for (const auto &[name, def] : stateDefMap) {
    (void)def;
    if (!statesSet.count(name)) { // 不可能路径，防御性保持一致
      diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::EqOverdetermined,
                     "对非状态量 \"" + name + "\" 的导数定义");
      ok = false;
    }
  }
  for (const auto &[name, def] : algDefMap) {
    (void)def;
    const SymbolInfo *info = table.find(name);
    if (info == nullptr)
      continue; // MC0102 已报
    if (info->kind != ast::Component::Kind::Variable) {
      diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::EqOverdetermined,
                     "不得对参数/常量 \"" + name + "\" 赋值（模型超定）");
      ok = false;
    }
  }

  // ---- 代数依赖图（仅代数量之间；状态作为 RHS 输入不参与）----
  std::unordered_map<std::string, std::set<std::string>> deps;
  for (const auto &[name, def] : algDefMap) {
    std::set<std::string> used;
    if (def != nullptr)
      collectIdents(*def, &used);
    for (const auto &u : used) {
      if (u == name || algDefMap.count(u))
        deps[name].insert(u);
    }
  }

  EquationAnalysis analysis;
  bool hasLoop = false;
  {
    std::vector<std::string> nodes;
    nodes.reserve(algDefMap.size());
    for (const auto &[name, def] : algDefMap) {
      (void)def;
      nodes.push_back(name);
    }
    std::vector<std::string> members;
    if (hasAlgebraicLoop(nodes, deps, &members)) {
      hasLoop = true;
      std::sort(members.begin(), members.end());
      std::string joined;
      for (size_t i = 0; i < members.size(); ++i) {
        joined += members[i];
        if (i + 1 < members.size())
          joined += " ↔ ";
      }
      diags.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::EqAlgebraicLoop,
                     "代数环检测: " + joined);
    }
  }
  const bool ok2 = !hasLoop;
  if (!ok2)
    ok = false;

  // ---- 状态初始化检查（MC0304）：需要显式 start，或 fixed=true（start 缺省 0.0）----
  for (const auto &comp : model.components) {
    if (comp.kind != ast::Component::Kind::Variable)
      continue;
    if (!statesSet.count(comp.nameTok.lexeme))
      continue;
    const bool fixedTrue = comp.hasFixed && comp.fixed &&
                           comp.fixed->kind == ast::ExprKind::BoolLit && comp.fixed->boolValue;
    if (!comp.start && !fixedTrue) {
      diags.addError(Location{"", comp.nameTok.line, comp.nameTok.col}, Code::InitMissingStart,
                     "状态量 \"" + comp.nameTok.lexeme + "\" 缺少 start 初值且未声明 fixed=true");
      ok = false;
    }
  }

  // ---- 实验注释校验（MC0401）----
  double startTime = 0.0, stopTime = 0.0, interval = 0.0;
  if (!model.experiment) {
    diags.addError(Location{"", model.nameTok.line, model.nameTok.col}, Code::ExperimentInvalid,
                   "缺少实验设置注释 annotation(experiment(StartTime=…, StopTime=…, Interval=…))");
    ok = false;
  } else {
    startTime = model.experiment->startTime;
    stopTime = model.experiment->stopTime;
    interval = model.experiment->interval;
    if (!model.experiment->hasStopTime || !model.experiment->hasInterval ||
        !(stopTime > startTime) || !(interval > 0.0) || interval > (stopTime - startTime) + 1e-12) {
      diags.addError(Location{"", model.experiment->annTok.line, model.experiment->annTok.col},
                     Code::ExperimentInvalid,
                     "实验设置无效：需 StopTime > StartTime 且 0 < Interval ≤ 时间跨度");
      ok = false;
    }
  }

  if (!ok)
    return std::nullopt;

  // ---- 拓扑排序（Kahn）：deps[a] 包含 a 所依赖的代数量 ----
  std::vector<std::string> algebraicNames;
  algebraicNames.reserve(algDefMap.size());
  for (const auto &[name, def] : algDefMap) {
    (void)def;
    algebraicNames.push_back(name);
  }
  std::set<std::string> done;
  std::vector<std::pair<std::string, const ast::Expr *>> ordered;
  while (ordered.size() < algebraicNames.size()) {
    bool progressed = false;
    for (const auto &name : algebraicNames) {
      if (done.count(name))
        continue;
      const auto &ds = deps[name];
      const bool ready = std::all_of(ds.begin(), ds.end(), [&](const std::string &d) {
        return algDefMap.count(d) == 0 || done.count(d); // 依赖可以是状态/参数
      });
      if (ready) {
        ordered.emplace_back(name, algDefMap[name]);
        done.insert(name);
        progressed = true;
      }
    }
    if (!progressed)
      break; // 环已提前报告；防御性退出
  }

  analysis.states.reserve(statesSet.size());
  for (const auto &sym : table.all()) {
    if (statesSet.count(sym.name))
      analysis.states.push_back(sym.name);
  }
  for (const auto &sym : table.all()) {
    auto it = stateDefMap.find(sym.name);
    if (it != stateDefMap.end()) {
      analysis.stateDefs.emplace_back(it->first, it->second);
    }
  }
  analysis.algebraicSteps = std::move(ordered);
  return analysis;
}

} // namespace mcdc
