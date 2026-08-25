#pragma once
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "semantic/equations.h"
#include "semantic/symbols.h"

namespace mcdc {

// 标识符映射（contracts/generated-program-contract.md）：合法标识符直用；
// 与 C++ 关键字冲突时加后缀 "_"。运行时符号均在命名空间内，不参与映射。
std::string mapToCppIdentifier(const std::string &name);

// 编译期常量求值（参数/常量初始化、start 初值）。
// 支持：数值字面量、一元 +/-、四则运算、对其他 constant/parameter 的引用。
// 循环引用或非常量构造报 MC0201。
std::optional<double> evalConstExpr(const ast::Expr &expr, const ast::Model &model,
                                    const SymbolTable &table, std::set<std::string> *visiting,
                                    DiagnosticCollector &diags);

struct PlanVar {
  std::string sourceName;
  std::string cppName;
  ast::Component::DeclType type = ast::Component::DeclType::Real;
};

struct TranslationPlan {
  std::string modelName;

  // 参数与常量（声明序）：以字面量形式烘焙进生成代码。
  struct ConstantParam {
    PlanVar var;
    std::string initLiteral; // 已格式化（%.17g / 整数字面量 / true|false）
  };
  std::vector<ConstantParam> constantsParams;

  // 状态量（声明序）；initLiterals/stateRhs 与之平行。
  std::vector<PlanVar> states;
  std::vector<std::string> stateInitLiterals;
  std::vector<const ast::Expr *> stateRhs;

  // 代数量（拓扑序，先算依赖）。
  struct AlgStep {
    PlanVar var;
    const ast::Expr *rhs = nullptr;
  };
  std::vector<AlgStep> algebraic;

  // 输出变量 = 全部 Variable 类别分量，按源模型声明序（CSV 列序，确定性）。
  std::vector<PlanVar> outputOrder;

  double startTime = 0.0;
  double stopTime = 0.0;
  double interval = 0.0;
  long steps = 0;
};

// 将分析产物组装为代码生成输入。要求 analyzeEquations 已成功。
// 常量求值失败会经 diags 报告（MC0201 等）。
TranslationPlan buildPlan(const ast::Model &model, const SymbolTable &table,
                          const EquationAnalysis &analysis, DiagnosticCollector &diags);

} // namespace mcdc
