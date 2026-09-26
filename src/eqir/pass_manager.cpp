#include "eqir/pass_manager.h"

#include "eqir/alias.h"
#include "eqir/blt.h"
#include "eqir/lower.h"
#include "eqir/match.h"
#include "eqir/schedule.h"
#include "eqir/tear.h"

namespace mcdc::eqir {

std::optional<EqModule> runBackend(const ast::Model &model, const SymbolTable &table,
                                   DiagnosticCollector &diags) {
  auto mod = lower(model, table, diags);
  if (!mod)
    return std::nullopt;

  if (!matchEquations(*mod, diags))
    return std::nullopt;

  if (!eliminateAliases(*mod, diags))
    return std::nullopt;

  // Alias 后重新匹配剩余方程
  if (!matchEquations(*mod, diags))
    return std::nullopt;

  if (!computeBlt(*mod, diags))
    return std::nullopt;

  if (!tearLoops(*mod, diags))
    return std::nullopt;

  if (!buildSchedule(*mod, diags))
    return std::nullopt;

  return mod;
}

} // namespace mcdc::eqir
