#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <tuple>

#include "ast/ast.h"
#include "codegen/plan.h"
#include "diagnostics/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/equations.h"
#include "semantic/symbols.h"

namespace {

using namespace mcdc;

struct Analysis {
  std::optional<ast::Model> model;
  std::optional<SymbolTable> symbols;
  std::optional<EquationAnalysis> equations;
  DiagnosticCollector diags;
};

Analysis analyzeSrc(const std::string &src) {
  Analysis a;
  Lexer lexer(src);
  auto tokens = lexer.tokenize(a.diags);
  a.diags.fillMissingFile("t.mo");
  Parser parser(std::move(tokens), a.diags);
  a.model = parser.parseFile();
  if (!a.model)
    return a;
  a.symbols = SymbolTable::build(*a.model, a.diags);
  if (!a.symbols)
    return a;
  analyzeExpressions(*a.model, *a.symbols, a.diags);
  a.equations = analyzeEquations(*a.model, *a.symbols, a.diags);
  return a;
}

bool hasCode(const DiagnosticCollector &diags, Code code) {
  for (const auto &d : diags.all()) {
    if (d.code == code)
      return true;
  }
  return false;
}

const std::string kExperiment = "annotation(experiment(StopTime = 2, Interval = 0.5));";

TEST(SemanticSymbols, ReportsDuplicateDeclarationMC0101) {
  const auto a = analyzeSrc(R"(model M
  Real x;
  parameter Real x;
equation
end M;
)");
  EXPECT_TRUE(hasCode(a.diags, Code::SymbolDuplicate));
}

TEST(SemanticSymbols, ReportsUndeclaredIdentifierMC0102) {
  const auto a = analyzeSrc(R"(model M
  Real x(start = 1, fixed = true);
equation
  der(x) = y;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::SymbolUndeclared));
}

TEST(SemanticSymbols, ReportsConstantWithoutValueMC0103) {
  const auto a = analyzeSrc(R"(model M
  constant Real c;
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::SymbolConstantMissingValue));
}

TEST(SemanticSymbols, ReportsUnknownFunctionMC0202) {
  const auto a = analyzeSrc(R"(model M
  Real x(start = 1, fixed = true);
equation
  der(x) = foo(x);
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::ExprUnknownFunction));
}

TEST(SemanticSymbols, ReportsDerInRhsMC0203) {
  const auto a = analyzeSrc(R"(model M
  Real x(start = 1, fixed = true);
  Real y(start = 1, fixed = true);
equation
  der(x) = der(y);
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::ExprBadDerPlacement));
}

TEST(SemanticEquations, ReportsUnderdeterminedMC0301) {
  const auto a = analyzeSrc(R"(model M
  Real x(start = 1, fixed = true);
  Real y;
equation
  der(x) = 1;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqUnderdetermined));
}

TEST(SemanticEquations, ReportsOverdeterminedMC0302OnParamAssign) {
  const auto a = analyzeSrc(R"(model M
  parameter Real k = 1;
  Real x(start = 1, fixed = true);
equation
  der(x) = -k * x;
  k = 3;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqOverdetermined));
}

TEST(SemanticEquations, ReportsAlgebraicLoopMC0303) {
  const auto a = analyzeSrc(R"(model M
  Real s(start = 1, fixed = true);
  Real a;
  Real b;
equation
  der(s) = a + b;
  a = b + 1;
  b = a * 0.5;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqAlgebraicLoop));
}

TEST(SemanticEquations, ReportsSelfLoopMC0303) {
  const auto a = analyzeSrc(R"(model M
  Real s(start = 1, fixed = true);
  Real u;
equation
  der(s) = u;
  u = u + 1;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqAlgebraicLoop));
}

TEST(SemanticEquations, AllowsCoupledStatesWithoutLoop) {
  // 状态互相出现在 RHS 不构成代数环（由 RK4 同步积分）。
  const auto a = analyzeSrc(R"(model Osc
  parameter Real w = 2;
  Real x(start = 1, fixed = true);
  Real v(start = 0, fixed = true);
equation
  der(x) = v;
  der(v) = -w * w * x;
)" + kExperiment + "\nend Osc;\n");
  EXPECT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());
  EXPECT_EQ(a.equations->states.size(), 2u);
}

TEST(SemanticEquations, TopologicallySortsAlgebraicChain) {
  const auto a = analyzeSrc(R"(model Chain
  Real s(start = 1, fixed = true);
  Real u2;
  Real u1;
equation
  der(s) = u2;
  u2 = u1 + 1;   // 声明在依赖之后：拓扑序必须先算 u1
  u1 = s * 2;
)" + kExperiment + "\nend Chain;\n");
  ASSERT_TRUE(a.equations.has_value());
  ASSERT_EQ(a.equations->algebraicSteps.size(), 2u);
  EXPECT_EQ(a.equations->algebraicSteps[0].first, "u1");
  EXPECT_EQ(a.equations->algebraicSteps[1].first, "u2");
}

TEST(SemanticEquations, ReportsMissingStateStartMC0304) {
  const auto a = analyzeSrc(R"(model M
  Real x;
equation
  der(x) = 0;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::InitMissingStart));
}

TEST(SemanticEquations, AcceptsFixedTrueWithoutExplicitStart) {
  const auto a = analyzeSrc(R"(model M
  Real x(fixed = true);
equation
  der(x) = 0;
)" + kExperiment + "\nend M;\n");
  EXPECT_FALSE(a.diags.hasErrors());
}

TEST(SemanticEquations, ReportsMissingExperimentMC0401) {
  const auto a = analyzeSrc(R"(model M
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
end M;
)");
  EXPECT_TRUE(hasCode(a.diags, Code::ExperimentInvalid));
}

TEST(SemanticEquations, ReportsInvalidIntervalMC0401) {
  const auto a = analyzeSrc(R"(model M
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
  annotation(experiment(StopTime = 2, Interval = 5));
end M;
)");
  EXPECT_TRUE(hasCode(a.diags, Code::ExperimentInvalid));
}

TEST(PlanBuild, EvaluatesParameterExpressionsAndMapsIdentifiers) {
  auto a = analyzeSrc(R"(model M
  parameter Real k = 1 + 2 * a;
  parameter Real a = 2;
  Real switch(start = 1, fixed = true);
equation
  der(switch) = -k * switch;
)" + kExperiment + "\nend M;\n");
  ASSERT_FALSE(a.diags.hasErrors());
  auto plan = buildPlan(*a.model, *a.symbols, *a.equations, a.diags);
  ASSERT_EQ(plan.constantsParams.size(), 2u);
  EXPECT_EQ(plan.constantsParams[0].initLiteral, "5");
  EXPECT_EQ(plan.constantsParams[1].initLiteral, "2");
  // C++ 关键字冲突 → 后缀 "_"
  EXPECT_EQ(plan.outputOrder[0].cppName, "switch_");
}

} // namespace
