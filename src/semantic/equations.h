#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "semantic/symbols.h"

namespace mcdc {

// 方程级分析结果（data-model.md EquationAnalysis）。
struct EquationAnalysis {
  // 状态量（出现 der(x) 的变量），按源文件声明序。
  std::vector<std::string> states;
  // 状态量的导数定义：state -> rhs 表达式。
  std::vector<std::pair<std::string, const ast::Expr *>> stateDefs;
  // 代数量按拓扑序（先算依赖）：name -> rhs 表达式。
  std::vector<std::pair<std::string, const ast::Expr *>> algebraicSteps;
  // 条件方程归约产物的所有权（stateDefs/algebraicSteps 中的指针可能指向其中）。
  std::vector<ast::ExprPtr> synthesized;
};

// 配平（MC0301/MC0302）、代数环检测（MC0303）、状态初始化检查（MC0304）、
// 实验注释校验（MC0401）与代数方程拓扑排序。
std::optional<EquationAnalysis> analyzeEquations(const ast::Model &model, const SymbolTable &table,
                                                 DiagnosticCollector &diags);

} // namespace mcdc
