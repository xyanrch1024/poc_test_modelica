#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

namespace {

using namespace mcdc;

struct ParseResult {
  std::optional<ast::Model> model;
  DiagnosticCollector diags;
};

ParseResult parseSrc(const std::string &src) {
  ParseResult r;
  Lexer lexer(src);
  auto tokens = lexer.tokenize(r.diags);
  r.diags.fillMissingFile("t.mo");
  Parser parser(std::move(tokens), r.diags);
  r.model = parser.parseFile();
  return r;
}

bool hasCode(const DiagnosticCollector &diags, Code code) {
  for (const auto &d : diags.all()) {
    if (d.code == code)
      return true;
  }
  return false;
}

const ast::Expr *exprOf(const ast::EquationSide &side) {
  return side.expr.get();
}

TEST(ParserValid, ParsesCoolingLikeModel) {
  const auto r = parseSrc(R"(model Cooling
  parameter Real k = 0.5;
  Real T(start = 90, fixed = true);
equation
  der(T) = -k * (T - 20);
  annotation(experiment(StartTime = 0, StopTime = 5, Interval = 0.01));
end Cooling;
)");
  ASSERT_TRUE(r.model.has_value());
  EXPECT_FALSE(r.diags.hasErrors());
  EXPECT_EQ(r.model->nameTok.lexeme, "Cooling");
  ASSERT_EQ(r.model->components.size(), 2u);
  EXPECT_EQ(r.model->components[0].kind, ast::Component::Kind::Parameter);
  EXPECT_EQ(r.model->components[1].kind, ast::Component::Kind::Variable);
  ASSERT_TRUE(r.model->components[1].hasStart);
  ASSERT_TRUE(r.model->components[1].hasFixed);
  ASSERT_EQ(r.model->equations.size(), 1u);
  EXPECT_TRUE(r.model->equations[0].lhs.isDer);
  EXPECT_EQ(r.model->equations[0].lhs.targetTok.lexeme, "T");
  ASSERT_TRUE(r.model->experiment.has_value());
  EXPECT_DOUBLE_EQ(r.model->experiment->stopTime, 5.0);
  EXPECT_DOUBLE_EQ(r.model->experiment->interval, 0.01);
}

TEST(ParserValid, RespectsOperatorPrecedence) {
  const auto r = parseSrc(R"(model M
  Real a; Real b; Real c;
equation
  a = b + c * 2;
  annotation(experiment(StopTime = 1, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.model.has_value());
  const ast::Expr *rhs = r.model->equations[0].rhs.expr.get();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->kind, ast::ExprKind::Binary);
  EXPECT_EQ(rhs->op, "+");
  ASSERT_EQ(rhs->rhs->kind, ast::ExprKind::Binary); // c*2 先结合
  EXPECT_EQ(rhs->rhs->op, "*");
}

TEST(ParserValid, ParsesUnaryParensCallsComparisons) {
  const auto r = parseSrc(R"(model M
  Real a; Real b; Boolean f;
equation
  a = -(b + 1) / 2;
  f = not (a > 0) or (a <= 1 and b <> 2);
  b = min(abs(a), max(a, sqrt(a)));
  annotation(experiment(StopTime = 1, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.model.has_value());
  EXPECT_FALSE(r.diags.hasErrors());
  // 第三个方程是函数调用
  const ast::Expr *call = r.model->equations[2].rhs.expr.get();
  ASSERT_EQ(call->kind, ast::ExprKind::Call);
  EXPECT_EQ(call->op, "min");
}

TEST(ParserValid, AcceptsUnitAttributeAndDescriptionStrings) {
  const auto r = parseSrc(R"(model M
  parameter Real k(unit = "1/s") = 2 "速率";
  Real x(start = 1, fixed = true);
equation
  der(x) = -k * x;
  annotation(experiment(StopTime = 1, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.model.has_value());
  EXPECT_FALSE(r.diags.hasErrors());
}

TEST(ParserErrors, EmptyOrCommentOnlyFileReportsMC0001) {
  const auto r = parseSrc("// 只有注释\n/* 块注释 */\n");
  EXPECT_FALSE(r.model.has_value());
  EXPECT_TRUE(hasCode(r.diags, Code::StructureEmptyFile));
}

TEST(ParserErrors, MultipleModelsReportMC0002) {
  const auto r = parseSrc(R"(model A
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
end A;
model B
  Real y(start = 1, fixed = true);
equation
  der(y) = 0;
end B;
)");
  // 解析继续收集错误，但必须报告多模型。
  EXPECT_TRUE(hasCode(r.diags, Code::StructureMultipleModels));
}

TEST(ParserErrors, UnsupportedConstructsReportMC0501WithLocation) {
  const std::vector<std::string> keywords = {
      "package", "import", "algorithm", "when",     "if",
      "connect", "reinit", "record",    "function", "each",
  };
  for (const auto &kw : keywords) {
    const std::string src = "model M\n  Real x(start = 1, fixed = true);\n" + kw +
                            " something;\nequation\n  der(x) = 0;\n"
                            "  annotation(experiment(StopTime = 1, Interval = 0.5));\nend M;\n";
    const auto r = parseSrc(src);
    bool found = false;
    int line = 0;
    for (const auto &d : r.diags.all()) {
      if (d.code == Code::UnsupportedConstruct) {
        found = true;
        line = d.location.line;
      }
    }
    EXPECT_TRUE(found) << "keyword: " << kw;
    if (found)
      EXPECT_EQ(line, 3) << "keyword: " << kw;
  }
}

TEST(ParserErrors, MissingSemicolonProducesDiagnostic) {
  const auto r = parseSrc(R"(model M
  Real a
  Real b;
equation
  a = b;
  annotation(experiment(StopTime = 1, Interval = 0.5));
end M;
)");
  EXPECT_FALSE(r.diags.all().empty());
}

TEST(ParserErrors, InvalidExperimentOptionReportsMC0401) {
  const auto r = parseSrc(R"(model M
  Real x(start = 1, fixed = true);
equation
  der(x) = 0;
  annotation(experiment(Bogus = 3, StopTime = 1, Interval = 0.5));
end M;
)");
  EXPECT_TRUE(hasCode(r.diags, Code::ExperimentInvalid));
}

TEST(ParserErrors, MultipleSyntaxErrorsCollectedInOnePassSorted) {
  const auto r = parseSrc(R"(model M
  Real a = ;
  Real b;
equation
  b = @;
  annotation(experiment(StopTime = 1, Interval = 0.5));
end M;
)");
  const auto sorted = r.diags.sorted();
  ASSERT_GE(sorted.size(), 2u);
  for (size_t i = 1; i < sorted.size(); ++i) {
    const auto &prev = sorted[i - 1].location;
    const auto &cur = sorted[i].location;
    ASSERT_FALSE(prev.file != cur.file   ? prev.file > cur.file
                 : prev.line != cur.line ? prev.line > cur.line
                                         : prev.col > cur.col)
        << "诊断未按位置排序";
  }
}

} // namespace
