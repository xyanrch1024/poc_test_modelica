#include "eqir/match.h"

#include <set>
#include <string>

namespace mcdc::eqir {

bool matchEquations(EqModule &mod, DiagnosticCollector &diags) {
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

} // namespace mcdc::eqir
