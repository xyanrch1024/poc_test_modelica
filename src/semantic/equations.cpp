#include "semantic/equations.h"

#include "eqir/pass_manager.h"

namespace mcdc {

std::optional<EquationAnalysis> analyzeEquations(const ast::Model &model, const SymbolTable &table,
                                                 DiagnosticCollector &diags) {
  auto mod = eqir::runBackend(model, table, diags);
  if (!mod)
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
  // 保留 extras 所有权在 eqModule 内
  analysis.eqModule = std::move(*mod);
  return analysis;
}

} // namespace mcdc
