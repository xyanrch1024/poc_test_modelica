#include "eqir/pass_manager.h"

#include "eqir/alias.h"
#include "eqir/blt.h"
#include "eqir/init_lower.h"
#include "eqir/lower.h"
#include "eqir/match.h"
#include "eqir/schedule.h"
#include "eqir/tear.h"

namespace mcdc::eqir {
namespace {

bool runPasses(EqModule &mod, DiagnosticCollector &diags) {
  if (!matchEquations(mod, diags))
    return false;
  if (!eliminateAliases(mod, diags))
    return false;
  if (!matchEquations(mod, diags))
    return false;
  if (!computeBlt(mod, diags))
    return false;
  if (!tearLoops(mod, diags))
    return false;
  if (!buildSchedule(mod, diags))
    return false;
  return true;
}

} // namespace

std::optional<EqModule> runBackend(const ast::Model &model, const SymbolTable &table,
                                   DiagnosticCollector &diags) {
  auto mod = lower(model, table, diags);
  if (!mod)
    return std::nullopt;
  if (!runPasses(*mod, diags))
    return std::nullopt;
  return mod;
}

std::optional<EqModule> runInitialBackend(const ast::Model &model, const SymbolTable &table,
                                          const EqModule &continuous,
                                          DiagnosticCollector &diags) {
  auto mod = lowerInitial(model, table, continuous, diags);
  if (!mod)
    return std::nullopt;
  if (!runPasses(*mod, diags))
    return std::nullopt;
  return mod;
}

} // namespace mcdc::eqir
