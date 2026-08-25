#include <gtest/gtest.h>

#include <algorithm>

#include "diagnostics/diagnostic.h"

namespace {

using namespace mcdc;

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

} // namespace
