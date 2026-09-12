#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>

#include "diagnostics/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/equations.h"
#include "semantic/symbols.h"

namespace {

using namespace mcdc;

// 跑完整词法+语法+语义流水线，返回全部诊断（不提前中断）。
std::vector<Diagnostic> analyzeAll(const std::string &src) {
  DiagnosticCollector diags;
  Lexer lexer(src);
  auto tokens = lexer.tokenize(diags);
  diags.fillMissingFile("multi.mo");
  Parser parser(std::move(tokens), diags);
  auto model = parser.parseFile();
  if (!model)
    return diags.sorted();
  auto symbols = SymbolTable::build(*model, diags);
  if (!symbols)
    return diags.sorted();
  analyzeExpressions(*model, *symbols, diags);
  analyzeEquations(*model, *symbols, diags);
  return diags.sorted();
}

Location loc(const std::string &file, int line, int col) {
  return Location{file, line, col};
}

TEST(DiagnosticsRender, FormatsErrorWithLocationAndCode) {
  Diagnostic d{Severity::Error, loc("cooling.mo", 5, 9), Code::SymbolUndeclared,
               "未声明的标识符 \"kappa\""};
  EXPECT_EQ(render(d), "[error] cooling.mo:5:9: 未声明的标识符 \"kappa\" [MC0102]");
}

TEST(DiagnosticsRender, FormatsWarningAndPadsCode) {
  Diagnostic d{Severity::Warning, loc("a.mo", 1, 1), Code::StructureEmptyFile, "空文件"};
  EXPECT_EQ(render(d), "[warning] a.mo:1:1: 空文件 [MC0001]");
}

TEST(DiagnosticsCollector, HasErrorsReflectsSeverity) {
  DiagnosticCollector c;
  c.addError(loc("f.mo", 2, 3), Code::SymbolDuplicate, "dup");
  EXPECT_TRUE(c.hasErrors());

  DiagnosticCollector w;
  w.addWarning(loc("f.mo", 2, 3), Code::ExprUnsupported, "warn");
  EXPECT_FALSE(w.hasErrors());
}

TEST(DiagnosticsCollector, SortedByFileLineCol) {
  DiagnosticCollector c;
  c.addError(loc("b.mo", 1, 5), Code::SymbolUndeclared, "x");
  c.addError(loc("a.mo", 9, 1), Code::ExprUnsupported, "y");
  c.addError(loc("b.mo", 1, 2), Code::ExprUnknownFunction, "z");

  const auto sorted = c.sorted();
  ASSERT_EQ(sorted.size(), 3u);
  EXPECT_EQ(sorted[0].location.file, "a.mo");
  EXPECT_EQ(sorted[1].location.line, 1);
  EXPECT_EQ(sorted[1].location.col, 2);
  EXPECT_EQ(sorted[2].location.col, 5);
}

TEST(DiagnosticsCollector, FillMissingFileOnlyTouchesEmptyNames) {
  DiagnosticCollector c;
  c.addError(Location{}, Code::ExprUnsupported, "lex");           // 词法阶段无文件名
  c.addError(loc("given.mo", 3, 1), Code::SymbolUndeclared, "p"); // 已带文件名
  c.fillMissingFile("cooling.mo");

  ASSERT_EQ(c.all().size(), 2u);
  EXPECT_EQ(c.all()[0].location.file, "cooling.mo");
  EXPECT_EQ(c.all()[1].location.file, "given.mo");
}

// ---- 条件构造多错误一次报全（spec 003 T027 / FR-005）----

TEST(DiagnosticsCollector, IfErrorsAllReportedInOnePassSorted) {
  // 同一文件 3 处 if 相关非法构造：语义阶段互不阻塞，一次输出全部。
  //   y = if t then 1 else 2;    → MC0204（条件非布尔）
  //   z = if b then 1 else true; → MC0205（分支类型不一致）
  //   if b then v = 1; end if;   → MC0305（缺 else 且非常量）
  const auto sorted = analyzeAll(R"(model M
  Real t;
  Real y;
  Boolean b;
  Real z;
  Real v;
equation
  y = if t then 1 else 2;
  z = if b then 1 else true;
  if b then
    v = 1;
  end if;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");

  bool has204 = false, has205 = false, has305 = false;
  for (const auto &d : sorted) {
    if (d.code == Code::ExprIfConditionNotBoolean)
      has204 = true;
    if (d.code == Code::ExprIfBranchTypeMismatch)
      has205 = true;
    if (d.code == Code::EqIfBranchMismatch)
      has305 = true;
  }
  EXPECT_TRUE(has204) << "MC0204（条件非布尔）应被报出";
  EXPECT_TRUE(has205) << "MC0205（分支类型不一致）应被报出";
  EXPECT_TRUE(has305) << "MC0305（缺 else 且非常量）应被报出";

  // sorted 按 (file,line,col) 升序：三处 if 落在同一文件，行号递增。
  std::vector<int> lines;
  for (const auto &d : sorted)
    lines.push_back(d.location.line);
  EXPECT_TRUE(std::is_sorted(lines.begin(), lines.end()));
}

} // namespace
