#include "eqir/match.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "eqir/expr_util.h"

namespace mcdc::eqir {
namespace {

std::set<std::string> incidenceVars(const Equation &eq, const EqModule &mod) {
  std::set<std::string> vars;
  vars.insert(eq.preferredUnknown);
  if (eq.rhs)
    collectIdents(*eq.rhs, &vars);
  // 只保留本模块未知量
  std::set<std::string> out;
  for (const auto &v : vars) {
    if (findUnknown(mod, v))
      out.insert(v);
  }
  // RHS 中的 der(x) 以 Ident "der(x)" 出现时已由 collectIdents 收集
  return out;
}

bool dfsMatch(size_t eqIdx, const std::vector<std::vector<size_t>> &adj,
              std::vector<int> *pairU, std::vector<int> *pairV, std::vector<char> *seen) {
  for (size_t v : adj[eqIdx]) {
    if ((*seen)[v])
      continue;
    (*seen)[v] = 1;
    if ((*pairV)[v] < 0 || dfsMatch(static_cast<size_t>((*pairV)[v]), adj, pairU, pairV, seen)) {
      (*pairU)[eqIdx] = static_cast<int>(v);
      (*pairV)[v] = static_cast<int>(eqIdx);
      return true;
    }
  }
  return false;
}

} // namespace

bool matchEquations(EqModule &mod, DiagnosticCollector &diags) {
  if (!mod.isInitial) {
    std::set<std::string> matchedVars;
    bool ok = true;
    for (auto &eq : mod.equations) {
      if (eq.isStateDeriv) {
        eq.matched = eq.preferredUnknown;
        continue;
      }
      const Unknown *u = findUnknown(mod, eq.preferredUnknown);
      if (u == nullptr || u->role != UnknownRole::Algebraic) {
        diags.addError(Location{"", eq.locTok.line, eq.locTok.col}, Code::EqOverdetermined,
                       "方程无法匹配到代数量 \"" + eq.preferredUnknown + "\"");
        ok = false;
        continue;
      }
      if (matchedVars.count(eq.preferredUnknown)) {
        diags.addError(Location{"", eq.locTok.line, eq.locTok.col}, Code::EqOverdetermined,
                       "变量 \"" + eq.preferredUnknown + "\" 被多个方程匹配");
        ok = false;
        continue;
      }
      eq.matched = eq.preferredUnknown;
      matchedVars.insert(eq.preferredUnknown);
    }
    for (const auto &u : mod.unknowns) {
      if (u.role != UnknownRole::Algebraic)
        continue;
      if (mod.aliases.count(u.name))
        continue;
      if (!matchedVars.count(u.name)) {
        diags.addError(Location{"", mod.modelTok.line, mod.modelTok.col}, Code::EqUnderdetermined,
                       "代数量 \"" + u.name + "\" 未被任何方程匹配");
        ok = false;
      }
    }
    return ok;
  }

  // ---- 初始系统：优先 preferred；冲突则 incidence 二分图匹配 ----
  std::map<std::string, size_t> varIndex;
  std::vector<std::string> varNames;
  for (const auto &u : mod.unknowns) {
    if (mod.aliases.count(u.name))
      continue;
    varIndex[u.name] = varNames.size();
    varNames.push_back(u.name);
  }

  std::vector<Equation *> algEqs;
  for (auto &eq : mod.equations)
    algEqs.push_back(&eq);

  // 尝试 preferred 无冲突
  {
    std::set<std::string> used;
    bool conflict = false;
    for (auto *eq : algEqs) {
      if (!varIndex.count(eq->preferredUnknown) || used.count(eq->preferredUnknown)) {
        conflict = true;
        break;
      }
      used.insert(eq->preferredUnknown);
    }
    if (!conflict && used.size() == varNames.size()) {
      for (auto *eq : algEqs)
        eq->matched = eq->preferredUnknown;
      return true;
    }
  }

  std::vector<std::vector<size_t>> adj(algEqs.size());
  for (size_t i = 0; i < algEqs.size(); ++i) {
    for (const auto &v : incidenceVars(*algEqs[i], mod)) {
      auto it = varIndex.find(v);
      if (it != varIndex.end())
        adj[i].push_back(it->second);
    }
    // 确定性：按声明序已体现在 varIndex；对邻接排序
    std::sort(adj[i].begin(), adj[i].end());
    adj[i].erase(std::unique(adj[i].begin(), adj[i].end()), adj[i].end());
  }

  std::vector<int> pairU(algEqs.size(), -1);
  std::vector<int> pairV(varNames.size(), -1);
  size_t matched = 0;
  for (size_t i = 0; i < algEqs.size(); ++i) {
    std::vector<char> seen(varNames.size(), 0);
    if (dfsMatch(i, adj, &pairU, &pairV, &seen))
      ++matched;
  }

  if (matched != algEqs.size() || matched != varNames.size()) {
    diags.addError(Location{"", mod.modelTok.line, mod.modelTok.col}, Code::InitUnderdetermined,
                   "初始系统结构匹配失败（方程与未知量无法一一对应）");
    return false;
  }

  for (size_t i = 0; i < algEqs.size(); ++i)
    algEqs[i]->matched = varNames[static_cast<size_t>(pairU[i])];
  return true;
}

} // namespace mcdc::eqir
