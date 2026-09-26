#pragma once
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "semantic/equations.h"
#include "semantic/symbols.h"

namespace mcdc {

// 标识符映射（contracts/generated-program-contract.md）：合法标识符直用；
// 与 C++ 关键字冲突时加后缀 "_"。运行时符号均在命名空间内，不参与映射。
std::string mapToCppIdentifier(const std::string &name);

// 编译期常量求值（参数/常量初始化、start 初值）。
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

  struct ConstantParam {
    PlanVar var;
    std::string initLiteral;
  };
  std::vector<ConstantParam> constantsParams;

  std::vector<PlanVar> states;
  std::vector<std::string> stateInitLiterals;
  std::vector<const ast::Expr *> stateRhs;

  // 代数步骤：赋值或 Newton 求解块。
  struct AlgStep {
    enum class Kind { Assign, Solve } kind = Kind::Assign;
    PlanVar var;                 // Assign
    const ast::Expr *rhs = nullptr;
    std::vector<PlanVar> tearVars;              // Solve
    std::vector<const ast::Expr *> residuals;   // Solve，各 == 0
  };
  std::vector<AlgStep> algebraic;

  // 被 alias 消除的变量：生成时在 compute_algebraic 末尾同步。
  struct AliasBind {
    PlanVar var;
    std::string canonicalCpp; // 非常量时
    bool isConst = false;
    std::string constLiteral;
    double scale = 1.0;
  };
  std::vector<AliasBind> aliasBinds;

  std::vector<PlanVar> outputOrder;

  double startTime = 0.0;
  double stopTime = 0.0;
  double interval = 0.0;
  long steps = 0;
};

TranslationPlan buildPlan(const ast::Model &model, const SymbolTable &table,
                          const EquationAnalysis &analysis, DiagnosticCollector &diags);

// 从 EqIR 调度结果构建代码生成计划。
TranslationPlan buildPlanFromEqModule(const ast::Model &model, const SymbolTable &table,
                                      const eqir::EqModule &mod, DiagnosticCollector &diags);

} // namespace mcdc
