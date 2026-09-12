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

// ---- 条件构造语义（spec 003 T007）----

TEST(SemanticConditional, AcceptsBooleanVariableCondition) {
  const auto a = analyzeSrc(R"(model M
  Real y;
  Real x(start = 1, fixed = true);
  Boolean b;
equation
  der(x) = 0;
  b = x > 0;
  y = if b then 1 else 2;
)" + kExperiment + "\nend M;\n");
  EXPECT_FALSE(a.diags.hasErrors());
}

TEST(SemanticConditional, AcceptsRelationAndNestedBooleanIfCondition) {
  // isBooleanExpr 闭包：关系比较、逻辑组合、嵌套布尔 if 表达式作条件。
  const auto a = analyzeSrc(R"(model M
  Real y;
  Real z;
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
  y = if (x > 0) and not (x == 0) then 1 else 2;
  z = if (if x > 1 then true else false) then 3 else 4;
)" + kExperiment + "\nend M;\n");
  EXPECT_FALSE(a.diags.hasErrors());
}

TEST(SemanticConditional, ReportsNonBooleanConditionMC0204) {
  const auto a = analyzeSrc(R"(model M
  Real y;
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
  y = if x then 1 else 2;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::ExprIfConditionNotBoolean));
}

TEST(SemanticConditional, ReportsNonBooleanConditionalEquationMC0204) {
  const auto a = analyzeSrc(R"(model M
  Real y;
  Real s(start = 1, fixed = true);
equation
  der(s) = 0;
  if s + 2 then
    y = 1;
  else
    y = 2;
  end if;
)" + kExperiment + "\nend M;\n");
  // cond = "s + 2"（数值）→ MC0204。
  EXPECT_TRUE(hasCode(a.diags, Code::ExprIfConditionNotBoolean));
}

TEST(SemanticConditional, ReportsBranchTypeMismatchMC0205) {
  const auto a = analyzeSrc(R"(model M
  Real y;
  Boolean b;
  parameter Real x = 1;
equation
  y = if b then 1 else true;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::ExprIfBranchTypeMismatch));
}

TEST(SemanticConditional, ReportsMissingElseMC0305) {
  const auto a = analyzeSrc(R"(model M
  Real y;
  Boolean b;
equation
  if b then
    y = 1;
  end if;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqIfBranchMismatch));
}

TEST(SemanticConditional, ReportsBranchEquationCountMC0305) {
  const auto a = analyzeSrc(R"(model M
  Real y;
  Boolean b;
equation
  if b then
    y = 1;
    y = 2;
  else
    y = 3;
  end if;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqIfBranchMismatch));
}

TEST(SemanticConditional, ReportsDifferentBranchUnknownsMC0305) {
  const auto a = analyzeSrc(R"(model M
  Real y;
  Real z;
  Boolean b;
equation
  if b then
    y = 1;
  else
    z = 2;
  end if;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqIfBranchMismatch));
}

TEST(SemanticConditional, ReducesConditionalEquationToTernary) {
  // case_23 形态：代数条件方程 + 状态量条件导数 + if 表达式。
  const auto a = analyzeSrc(R"(model M
  Real q;
  Real s(start = 0, fixed = true);
equation
  if s < 1 then
    q = 2 * s;
  else
    q = s + 1;
  end if;
  der(s) = if s < 1 then 1 else 2;
)" + kExperiment + "\nend M;\n");
  ASSERT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());

  // 代数 q 归约为 IfExpr。
  ASSERT_EQ(a.equations->algebraicSteps.size(), 1u);
  EXPECT_EQ(a.equations->algebraicSteps[0].first, "q");
  ASSERT_NE(a.equations->algebraicSteps[0].second, nullptr);
  EXPECT_EQ(a.equations->algebraicSteps[0].second->kind, ast::ExprKind::If);

  // 状态 s 的导数定义同样为 IfExpr。
  ASSERT_EQ(a.equations->stateDefs.size(), 1u);
  EXPECT_EQ(a.equations->stateDefs[0].first, "s");
  ASSERT_NE(a.equations->stateDefs[0].second, nullptr);
  EXPECT_EQ(a.equations->stateDefs[0].second->kind, ast::ExprKind::If);
}

TEST(SemanticConditional, UnionDependenciesOrderAlgebraicChain) {
  // D2：条件方程依赖 = 各分支并集；被依赖代数量必须先算（拓扑序）。
  const auto a = analyzeSrc(R"(model M
  Real s(start = 1, fixed = true);
  Real u2;
  Real u1;
equation
  der(s) = 0;
  if s > 0 then
    u2 = u1 + 1;
  else
    u2 = u1 - 1;
  end if;
  u1 = s * 2;
)" + kExperiment + "\nend M;\n");
  ASSERT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());
  ASSERT_EQ(a.equations->algebraicSteps.size(), 2u);
  EXPECT_EQ(a.equations->algebraicSteps[0].first, "u1");
  EXPECT_EQ(a.equations->algebraicSteps[1].first, "u2");
}

// ---- else if 链与嵌套（spec 003 T020）----

TEST(SemanticConditional, AcceptsElseifChainAndReducesRightAssoc) {
  // case_24 形态：三段 else if 链，归约为右结合嵌套三元。
  const auto a = analyzeSrc(R"(model M
  Real v;
  Real s(start = 0, fixed = true);
equation
  if s < 1 then
    v = 1;
  elseif s < 2 then
    v = 2;
  else
    v = 3;
  end if;
  der(s) = 0.5 * v;
)" + kExperiment + "\nend M;\n");
  ASSERT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());

  // 代数 v 归约为 If 三元；elseExpr 上又有构造成员：C0 ? 1 : (C1 ? 2 : 3)。
  ASSERT_EQ(a.equations->algebraicSteps.size(), 1u);
  EXPECT_EQ(a.equations->algebraicSteps[0].first, "v");
  ASSERT_NE(a.equations->algebraicSteps[0].second, nullptr);
  const auto *v = a.equations->algebraicSteps[0].second;
  EXPECT_EQ(v->kind, ast::ExprKind::If);
  ASSERT_NE(v->thenExpr, nullptr);
  EXPECT_EQ(v->thenExpr->kind, ast::ExprKind::NumLit); // "1"
  ASSERT_NE(v->elseExpr, nullptr);
  EXPECT_EQ(v->elseExpr->kind, ast::ExprKind::If); // 嵌套 C1 ? 2 : 3
  ASSERT_NE(v->elseExpr->thenExpr, nullptr);
  EXPECT_EQ(v->elseExpr->thenExpr->kind, ast::ExprKind::NumLit); // "2"
}

TEST(SemanticConditional, ChecksEveryElseifSegmentConditionMC0204) {
  // 链中每一段条件（含 elseif 段）都必须通过 MC0204 校验。
  const auto a = analyzeSrc(R"(model M
  Real v;
  Real s(start = 0, fixed = true);
equation
  if s < 1 then
    v = 1;
  elseif s then
    v = 2;
  end if;
  der(s) = 0.5 * v;
)" + kExperiment + "\nend M;\n");
  // 缺 else + 运行期 elseif 条件 → 依序走到 MC0305；此处应已由 MC0204 拦截。
  EXPECT_TRUE(hasCode(a.diags, Code::ExprIfConditionNotBoolean));
  EXPECT_TRUE(hasCode(a.diags, Code::EqIfBranchMismatch));
}

TEST(SemanticConditional, ChecksNestedTernaryBranchTypesMC0205) {
  // 嵌套三元各层分支类型必须一致：内层 then 数值 / else 布尔 → MC0205。
  const auto a = analyzeSrc(R"(model M
  Real y;
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
  y = if x < 2 then (if x < 1 then 3 else true) else 4;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::ExprIfBranchTypeMismatch));
}

TEST(SemanticConditional, NestedConditionalEquationSameUnknownCovered) {
  // 嵌套条件方程：内层归约后 target 仍是 v，与外层分支一致 → 合法。
  const auto a = analyzeSrc(R"(model M
  Real s(start = 1, fixed = true);
  Real v;
  Boolean b;
  Boolean c;
equation
  der(s) = 0;
  b = s > 2;
  c = s > 3;
  if b then
    if c then
      v = 1;
    else
      v = 2;
    end if;
  else
    v = 3;
  end if;
)" + kExperiment + "\nend M;\n");
  EXPECT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());
  // 代数量 b、c（条件用）、v（嵌套条件方程）各一步。
  ASSERT_EQ(a.equations->algebraicSteps.size(), 3u);
  EXPECT_EQ(a.equations->algebraicSteps[2].first, "v");
  ASSERT_NE(a.equations->algebraicSteps[2].second, nullptr);
  EXPECT_EQ(a.equations->algebraicSteps[2].second->kind, ast::ExprKind::If);
}

// ---- 常量条件静态折叠（spec 003 T031）----

TEST(SemanticConditional, ConstantTrueConditionFoldsThenBranch) {
  const auto a = analyzeSrc(R"(model M
  Real v;
  constant Boolean flag = true;
equation
  if flag then
    v = 7;
  else
    v = 8;
  end if;
)" + kExperiment + "\nend M;\n");
  ASSERT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());
  ASSERT_EQ(a.equations->algebraicSteps.size(), 1u);
  EXPECT_EQ(a.equations->algebraicSteps[0].first, "v");
  ASSERT_NE(a.equations->algebraicSteps[0].second, nullptr);
  // 全程是常量条件 → 直选 then 分支（数字 7），不再产生三元。
  EXPECT_EQ(a.equations->algebraicSteps[0].second->kind, ast::ExprKind::NumLit);
  EXPECT_NEAR(a.equations->algebraicSteps[0].second->numValue, 7.0, 1e-12);
}

TEST(SemanticConditional, ConstantFalseConditionFoldsElseBranch) {
  const auto a = analyzeSrc(R"(model M
  Real v;
  constant Boolean flag = 1 < 0;
equation
  if flag then
    v = 7;
  else
    v = 8;
  end if;
)" + kExperiment + "\nend M;\n");
  ASSERT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());
  ASSERT_EQ(a.equations->algebraicSteps.size(), 1u);
  ASSERT_NE(a.equations->algebraicSteps[0].second, nullptr);
  EXPECT_EQ(a.equations->algebraicSteps[0].second->kind, ast::ExprKind::NumLit);
  EXPECT_NEAR(a.equations->algebraicSteps[0].second->numValue, 8.0, 1e-12);
}

TEST(SemanticConditional, ConstantConditionAllowsMissingElse) {
  // 缺 else 仅当条件为编译期常量时允许（MC0306 具象化：静态选择活跃分支）。
  const auto a = analyzeSrc(R"(model M
  Real v;
  constant Boolean flag = true;
equation
  if flag then
    v = 7;
  end if;
)" + kExperiment + "\nend M;\n");
  EXPECT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations.has_value());
  ASSERT_EQ(a.equations->algebraicSteps.size(), 1u);
  ASSERT_NE(a.equations->algebraicSteps[0].second, nullptr);
  EXPECT_EQ(a.equations->algebraicSteps[0].second->kind, ast::ExprKind::NumLit);
  EXPECT_NEAR(a.equations->algebraicSteps[0].second->numValue, 7.0, 1e-12);
}

TEST(SemanticConditional, ConstantFalseMissingElseRejected) {
  // 常量条件恒假且缺 else：未知量永不被定义（沿用 MC0305 拒绝）。
  const auto a = analyzeSrc(R"(model M
  Real v;
  constant Boolean flag = false;
equation
  if flag then
    v = 7;
  end if;
)" + kExperiment + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::EqIfBranchMismatch));
}

} // namespace
