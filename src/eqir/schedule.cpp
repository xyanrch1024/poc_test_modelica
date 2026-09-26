#include "eqir/schedule.h"

#include <map>

#include "eqir/expr_util.h"

namespace mcdc::eqir {

bool buildSchedule(EqModule &mod, DiagnosticCollector &diags) {
  mod.schedule.clear();

  std::map<size_t, Equation *> byId;
  for (auto &eq : mod.equations)
    byId[eq.id] = &eq;

  auto pushSolve = [&](const std::vector<std::string> &tears, const std::vector<size_t> &eqIds) {
    SchedStep step;
    step.kind = StepKind::Solve;
    step.solve.tearVars = tears;
    for (size_t id : eqIds) {
      Equation *eq = byId[id];
      if (eq == nullptr || !eq->rhs)
        continue;
      // 残差始终相对 preferred（方程 LHS），与匹配无关
      auto residual = makeResidual(eq->locTok, eq->preferredUnknown, *eq->rhs);
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
    return true;
  };

  for (const auto &block : mod.blocks) {
    if (block.trivial) {
      if (block.unknowns.empty() || block.eqIds.empty())
        continue;
      Equation *eq = byId[block.eqIds[0]];
      if (eq == nullptr || !eq->matched || !eq->rhs)
        continue;
      // 匹配到 preferred → 直接赋值；否则一维 Newton
      if (*eq->matched == eq->preferredUnknown) {
        SchedStep step;
        step.kind = StepKind::Assign;
        step.assign.var = *eq->matched;
        step.assign.rhs = eq->rhs.get();
        mod.schedule.push_back(std::move(step));
      } else {
        if (!pushSolve(block.unknowns, block.eqIds))
          return false;
      }
      continue;
    }

    if (!pushSolve(block.unknowns, block.eqIds))
      return false;
  }
  return true;
}

} // namespace mcdc::eqir
