#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "cli/pipeline.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "eqir/pass_manager.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/equations.h"
#include "semantic/symbols.h"

namespace {

using namespace mcdc;

const std::string kExp = "annotation(experiment(StopTime = 1, Interval = 0.1));";

struct Packed {
  std::optional<ast::Model> model;
  std::optional<SymbolTable> symbols;
  std::optional<EquationAnalysis> equations;
  DiagnosticCollector diags;
};

Packed analyze(const std::string &src) {
  Packed a;
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

TEST(InitParser, ParsesInitialEquationSection) {
  Lexer lexer(R"(model M
  Real x(start = 0, fixed = true);
equation
  der(x) = -x;
initial equation
  der(x) = 0;
)" + kExp + "\nend M;\n");
  DiagnosticCollector diags;
  auto tokens = lexer.tokenize(diags);
  Parser parser(std::move(tokens), diags);
  auto model = parser.parseFile();
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->equations.size(), 1u);
  ASSERT_EQ(model->initialEquations.size(), 1u);
  EXPECT_TRUE(model->initialEquations[0].lhs.isDer);
}

TEST(InitSemantics, FixedTrueAddsInitEquation) {
  const auto a = analyze(R"(model M
  Real x(start = 3, fixed = true);
equation
  der(x) = -x;
)" + kExp + "\nend M;\n");
  EXPECT_FALSE(a.diags.hasErrors());
  ASSERT_TRUE(a.equations && a.equations->initModule);
  bool sawAssignX = false;
  for (const auto &step : a.equations->initModule->schedule) {
    if (step.kind == eqir::StepKind::Assign && step.assign.var == "x")
      sawAssignX = true;
  }
  EXPECT_TRUE(sawAssignX);
}

TEST(InitSemantics, FixedFalseWithoutInitialIsUnderdetermined) {
  const auto a = analyze(R"(model M
  Real x(start = 3);
equation
  der(x) = -x;
)" + kExp + "\nend M;\n");
  EXPECT_TRUE(hasCode(a.diags, Code::InitUnderdetermined));
}

TEST(InitSemantics, SteadyStateInitialEquation) {
  const auto a = analyze(R"(model M
  Real x(start = 1);
equation
  der(x) = -x;
initial equation
  der(x) = 0;
)" + kExp + "\nend M;\n");
  EXPECT_FALSE(a.diags.hasErrors()) << "unexpected diagnostics";
  ASSERT_TRUE(a.equations && a.equations->initModule);
}

TEST(InitEndToEnd, SteadyStateStartsAtZero) {
  DiagnosticCollector diags;
  const std::string src = R"(model InitSteady
  Real x(start = 1.0);
equation
  der(x) = -x;
initial equation
  der(x) = 0;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));
end InitSteady;
)";
  const std::string outDir = "init_steady_gen_tmp";
  auto outcome = runTranslate(src, "InitSteady.mo", &outDir, diags);
  ASSERT_TRUE(outcome.ok) << "translate failed";
  // 构建并运行
  ASSERT_EQ(0, std::system(("cmake -S " + outDir + " -B " + outDir +
                            "/build -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1")
                               .c_str()));
  ASSERT_EQ(0, std::system(("cmake --build " + outDir + "/build --parallel >/dev/null 2>&1").c_str()));
  ASSERT_EQ(0, std::system(("cd " + outDir + "/build && ./InitSteady >/dev/null 2>&1").c_str()));
  std::ifstream in(outDir + "/build/InitSteady_result.csv");
  ASSERT_TRUE(in.good());
  std::string header, first;
  std::getline(in, header);
  std::getline(in, first);
  // time,x → x(0) ≈ 0
  const auto comma = first.find(',');
  ASSERT_NE(comma, std::string::npos);
  const double x0 = std::stod(first.substr(comma + 1));
  EXPECT_NEAR(x0, 0.0, 1e-6);
  std::filesystem::remove_all(outDir);
}

} // namespace
