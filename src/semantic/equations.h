#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "semantic/symbols.h"

namespace mcdc {

struct EquationAnalysis {
  std::vector<std::string> states;
  std::vector<std::pair<std::string, const ast::Expr *>> stateDefs;
  std::vector<std::pair<std::string, const ast::Expr *>> algebraicSteps;
  std::vector<ast::ExprPtr> synthesized;

  std::optional<eqir::EqModule> eqModule;
  std::optional<eqir::EqModule> initModule;
};

std::optional<EquationAnalysis> analyzeEquations(const ast::Model &model, const SymbolTable &table,
                                                 DiagnosticCollector &diags);

} // namespace mcdc
