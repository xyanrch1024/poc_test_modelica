#include "eqir/blt.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include "eqir/expr_util.h"

namespace mcdc::eqir {

bool computeBlt(EqModule &mod, DiagnosticCollector &diags) {
  // 代数未知量集合（未被 alias 消除的）
  std::set<std::string> algSet;
  for (const auto &u : mod.unknowns) {
    if (u.role == UnknownRole::Algebraic && !mod.aliases.count(u.name))
      algSet.insert(u.name);
  }

  // var → matched eq id
  std::map<std::string, size_t> varToEq;
  for (const auto &eq : mod.equations) {
    if (eq.isStateDeriv || !eq.matched)
      continue;
    if (!algSet.count(*eq.matched))
      continue;
    varToEq[*eq.matched] = eq.id;
  }

  // deps[v] = 残差 incidence 中除 v 外的代数量；若 RHS 引用自身则保留自环。
  std::unordered_map<std::string, std::set<std::string>> deps;
  for (const auto &name : algSet) {
    auto it = varToEq.find(name);
    if (it == varToEq.end())
      continue;
    const Equation *eq = findEquation(mod, it->second);
    if (eq == nullptr || !eq->rhs)
      continue;
    std::set<std::string> rhsVars;
    collectIdents(*eq->rhs, &rhsVars);
    std::set<std::string> used = rhsVars;
    used.insert(eq->preferredUnknown);
    for (const auto &w : used) {
      if (w != name && algSet.count(w))
        deps[name].insert(w);
    }
    if (rhsVars.count(name))
      deps[name].insert(name); // 自环 u = f(u)
  }

  // Tarjan，按声明序启动以保证确定性
  std::vector<std::string> nodes(algSet.begin(), algSet.end());
  std::sort(nodes.begin(), nodes.end(), [&](const std::string &a, const std::string &b) {
    const Unknown *ua = findUnknown(mod, a);
    const Unknown *ub = findUnknown(mod, b);
    return (ua ? ua->declOrder : 0) < (ub ? ub->declOrder : 0);
  });

  std::unordered_map<std::string, int> index, lowlink;
  std::unordered_map<std::string, bool> onStack;
  std::vector<std::string> stack;
  int counter = 0;
  std::vector<std::vector<std::string>> sccs; // 发现序（逆拓扑）

  std::function<void(const std::string &)> strongconnect = [&](const std::string &v) {
    index[v] = lowlink[v] = counter++;
    stack.push_back(v);
    onStack[v] = true;
    for (const auto &w : deps[v]) {
      if (index.find(w) == index.end()) {
        strongconnect(w);
        lowlink[v] = std::min(lowlink[v], lowlink[w]);
      } else if (onStack[w]) {
        lowlink[v] = std::min(lowlink[v], index[w]);
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
      sccs.push_back(std::move(scc));
    }
  };

  for (const auto &n : nodes) {
    if (index.find(n) == index.end())
      strongconnect(n);
  }

  // Tarjan 弹出序：依赖目标先弹出，已是求值拓扑序（勿反转）。
  mod.blocks.clear();
  bool ok = true;
  for (auto &scc : sccs) {
    // 稳定未知量序：声明序
    std::sort(scc.begin(), scc.end(), [&](const std::string &a, const std::string &b) {
      const Unknown *ua = findUnknown(mod, a);
      const Unknown *ub = findUnknown(mod, b);
      return (ua ? ua->declOrder : 0) < (ub ? ub->declOrder : 0);
    });

    StrongComponent block;
    block.unknowns = scc;
    for (const auto &name : scc) {
      auto it = varToEq.find(name);
      if (it != varToEq.end())
        block.eqIds.push_back(it->second);
    }
    const bool selfLoop =
        scc.size() == 1 && deps.count(scc[0]) && deps.at(scc[0]).count(scc[0]);
    block.trivial = (scc.size() == 1 && !selfLoop);

    // 隐式 der：代数 SCC 不应含状态导数方程（防御）
    for (size_t id : block.eqIds) {
      const Equation *eq = findEquation(mod, id);
      if (eq && eq->isStateDeriv) {
        diags.addError(Location{"", mod.modelTok.line, mod.modelTok.col},
                       Code::EqImplicitUnsupported,
                       "不支持含 der(...) 的隐式代数结构");
        ok = false;
      }
    }
    mod.blocks.push_back(std::move(block));
  }
  return ok;
}

} // namespace mcdc::eqir
