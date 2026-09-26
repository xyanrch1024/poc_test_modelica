#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "eqir/pass_manager.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/symbols.h"

namespace {

using namespace mcdc;

std::optional<eqir::EqModule> backend(const std::string &src, DiagnosticCollector *diags) {
  Lexer lexer(src);
  auto tokens = lexer.tokenize(*diags);
  diags->fillMissingFile("t.mo");
  Parser parser(std::move(tokens), *diags);
  auto model = parser.parseFile();
  if (!model)
    return std::nullopt;
  auto symbols = SymbolTable::build(*model, *diags);
  if (!symbols)
    return std::nullopt;
  analyzeExpressions(*model, *symbols, *diags);
  return eqir::runBackend(*model, *symbols, *diags);
}

const std::string kExp = "annotation(experiment(StopTime = 1, Interval = 0.1));";

TEST(EqirAlias, EliminatesLaterDeclaredIdentityAlias) {
  DiagnosticCollector diags;
  auto mod = backend(R"(model M
  Real s(start = 1, fixed = true);
  Real x;
  Real y;
equation
  der(s) = x;
  x = s + 1;
  y = x;
)" + kExp + "\nend M;\n",
                     &diags);
  ASSERT_TRUE(mod.has_value()) << "unexpected errors";
  EXPECT_TRUE(mod->aliases.count("y"));
  EXPECT_EQ(mod->aliases.at("y").canonical, "x");
  EXPECT_EQ(mod->aliases.at("y").scale, 1.0);
  // schedule 不应再含 y 的赋值
  for (const auto &step : mod->schedule) {
    if (step.kind == eqir::StepKind::Assign)
      EXPECT_NE(step.assign.var, "y");
  }
}

TEST(EqirAlias, EliminatesNegatedAlias) {
  DiagnosticCollector diags;
  auto mod = backend(R"(model M
  Real s(start = 1, fixed = true);
  Real x;
  Real y;
equation
  der(s) = x;
  x = s;
  y = -x;
)" + kExp + "\nend M;\n",
                     &diags);
  ASSERT_TRUE(mod.has_value());
  ASSERT_TRUE(mod->aliases.count("y"));
  EXPECT_EQ(mod->aliases.at("y").canonical, "x");
  EXPECT_EQ(mod->aliases.at("y").scale, -1.0);
}

TEST(EqirTear, TearsTwoVarAlgebraicLoop) {
  DiagnosticCollector diags;
  auto mod = backend(R"(model M
  Real s(start = 1, fixed = true);
  Real a;
  Real b;
equation
  der(s) = a + b;
  a = b + 1;
  b = a * 0.5;
)" + kExp + "\nend M;\n",
                     &diags);
  ASSERT_TRUE(mod.has_value());
  bool saw = false;
  for (const auto &step : mod->schedule) {
    if (step.kind == eqir::StepKind::Solve) {
      saw = true;
      ASSERT_EQ(step.solve.tearVars.size(), 2u);
      EXPECT_EQ(step.solve.tearVars[0], "a");
      EXPECT_EQ(step.solve.tearVars[1], "b");
      EXPECT_EQ(step.solve.residuals.size(), 2u);
    }
  }
  EXPECT_TRUE(saw);
}

TEST(EqirBlt, OrdersIndependentBeforeDependent) {
  DiagnosticCollector diags;
  auto mod = backend(R"(model Chain
  Real s(start = 1, fixed = true);
  Real u2;
  Real u1;
equation
  der(s) = u2;
  u2 = u1 + 1;
  u1 = s * 2;
)" + kExp + "\nend Chain;\n",
                     &diags);
  ASSERT_TRUE(mod.has_value());
  ASSERT_GE(mod->schedule.size(), 2u);
  EXPECT_EQ(mod->schedule[0].assign.var, "u1");
  EXPECT_EQ(mod->schedule[1].assign.var, "u2");
}

} // namespace
