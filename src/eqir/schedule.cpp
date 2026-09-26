#include "eqir/schedule.h"

#include <map>

#include "eqir/expr_util.h"

namespace mcdc::eqir {

bool buildSchedule(EqModule &mod, DiagnosticCollector &diags) {
  (void)diags;
  mod.schedule.clear();

  std::map<size_t, Equation *> byId;
  for (auto &eq : mod.equations)
    byId[eq.id] = &eq;

  for (const auto &block : mod.blocks) {
    if (block.trivial) {
      if (block.unknowns.empty() || block.eqIds.empty())
        continue;
      Equation *eq = byId[block.eqIds[0]];
      if (eq == nullptr || !eq->matched || !eq->rhs)
        continue;
      SchedStep step;
      step.kind = StepKind::Assign;
      step.assign.var = *eq->matched;
      step.assign.rhs = eq->rhs.get();
      mod.schedule.push_back(std::move(step));
      continue;
    }

    // 非平凡：全 tear → Solve
    SchedStep step;
    step.kind = StepKind::Solve;
    step.solve.tearVars = block.unknowns;
    for (size_t id : block.eqIds) {
      Equation *eq = byId[id];
      if (eq == nullptr || !eq->matched || !eq->rhs)
        continue;
      auto residual = makeResidual(eq->locTok, *eq->matched, *eq->rhs);
      const ast::Expr *ptr = residual.get();
      mod.extras.push_back(std::move(residual));
      step.solve.residuals.push_back(ptr);
    }
    if (step.solve.tearVars.size() != step.solve.residuals.size()) {
      diags.addError(Location{"", mod.modelTok.line, mod.modelTok.col}, Code::EqAlgebraicLoop,
                     "代数环 tearing 失败：残差数与 tear 变量数不一致");
      return false;
    }
    mod.schedule.push_back(std::move(step));
  }
  return true;
}

} // namespace mcdc::eqir
