#include "semantic/equations.h"

#include "eqir/pass_manager.h"

namespace mcdc {

std::optional<EquationAnalysis> analyzeEquations(const ast::Model &model, const SymbolTable &table,
                                                 DiagnosticCollector &diags) {
  auto mod = eqir::runBackend(model, table, diags);
  if (!mod)
    return std::nullopt;

  auto init = eqir::runInitialBackend(model, table, *mod, diags);
  if (!init)
    return std::nullopt;

  EquationAnalysis analysis;
  analysis.states = mod->states;
  for (const auto &name : mod->states) {
    auto it = mod->stateRhs.find(name);
    if (it != mod->stateRhs.end())
      analysis.stateDefs.emplace_back(name, it->second);
  }
  for (const auto &step : mod->schedule) {
    if (step.kind == eqir::StepKind::Assign)
      analysis.algebraicSteps.emplace_back(step.assign.var, step.assign.rhs);
  }
  analysis.eqModule = std::move(*mod);
  analysis.initModule = std::move(*init);
  return analysis;
}

} // namespace mcdc
