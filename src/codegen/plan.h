#pragma once
#include <map>
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

std::string mapToCppIdentifier(const std::string &name);

// 初始未知量名（含 der(x)）→ C++ 标识符。
std::string mapInitUnknownToCpp(const std::string &name);

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
  std::vector<std::string> stateInitLiterals; // 兼容旧路径；新路径由 initialize 覆盖
  std::vector<const ast::Expr *> stateRhs;

  struct AlgStep {
    enum class Kind { Assign, Solve } kind = Kind::Assign;
    PlanVar var;
    const ast::Expr *rhs = nullptr;
    std::vector<PlanVar> tearVars;
    std::vector<const ast::Expr *> residuals;
  };
  std::vector<AlgStep> algebraic;

  struct AliasBind {
    PlanVar var;
    std::string canonicalCpp;
    bool isConst = false;
    std::string constLiteral;
    double scale = 1.0;
  };
  std::vector<AliasBind> aliasBinds;

  // 初始系统
  std::vector<AlgStep> initSteps;
  std::vector<AliasBind> initAliasBinds;
  std::map<std::string, double> initGuesses; // sourceName → guess
  std::vector<std::string> initDerUnknowns;  // "der(x)" 列表

  std::vector<PlanVar> outputOrder;

  double startTime = 0.0;
  double stopTime = 0.0;
  double interval = 0.0;
  long steps = 0;
};

TranslationPlan buildPlan(const ast::Model &model, const SymbolTable &table,
                          const EquationAnalysis &analysis, DiagnosticCollector &diags);

TranslationPlan buildPlanFromEqModules(const ast::Model &model, const SymbolTable &table,
                                       const eqir::EqModule &cont, const eqir::EqModule &init,
                                       DiagnosticCollector &diags);

} // namespace mcdc
