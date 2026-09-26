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

// 方程级分析结果（由 EqIR backend 填充，保持测试兼容）。
struct EquationAnalysis {
  std::vector<std::string> states;
  std::vector<std::pair<std::string, const ast::Expr *>> stateDefs;
  std::vector<std::pair<std::string, const ast::Expr *>> algebraicSteps;
  std::vector<ast::ExprPtr> synthesized;

  // EqIR 全量结果（pipeline/codegen 优先使用）。
  std::optional<eqir::EqModule> eqModule;
};

// 配平、状态初始化、实验校验 + EqIR 优化管线（匹配/BLT/alias/tearing/调度）。
std::optional<EquationAnalysis> analyzeEquations(const ast::Model &model, const SymbolTable &table,
                                                 DiagnosticCollector &diags);

} // namespace mcdc
