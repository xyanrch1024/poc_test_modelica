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
      "package", "import", "algorithm", "when",
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

// ---- 条件构造解析（spec 003 T006）----

TEST(ParserConditional, ParsesIfExpressionTernary) {
  const auto r = parseSrc(R"(model M
  Real y;
  Real x(start = 1, fixed = true);
equation
  y = if x > 0 then 1 else 2;
  der(x) = 0;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.diags.all().empty());
  ASSERT_TRUE(r.model.has_value());
  ASSERT_EQ(r.model->equations.size(), 2u);
  const auto &rhs = r.model->equations[0].rhs;
  ASSERT_NE(rhs.expr.get(), nullptr);
  EXPECT_EQ(rhs.expr->kind, ast::ExprKind::If);
  ASSERT_NE(rhs.expr->cond.get(), nullptr);
  EXPECT_EQ(rhs.expr->cond->kind, ast::ExprKind::Binary); // x > 0
  ASSERT_NE(rhs.expr->thenExpr.get(), nullptr);
  EXPECT_EQ(rhs.expr->thenExpr->kind, ast::ExprKind::NumLit); // 1
  ASSERT_NE(rhs.expr->elseExpr.get(), nullptr);
  EXPECT_EQ(rhs.expr->elseExpr->kind, ast::ExprKind::NumLit); // 2
}

TEST(ParserConditional, ParsesConditionalEquationWithTwoBranches) {
  const auto r = parseSrc(R"(model M
  Real v;
  Boolean b;
equation
  if b then
    v = 1;
  else
    v = 2;
  end if;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.diags.all().empty());
  ASSERT_TRUE(r.model.has_value());
  ASSERT_EQ(r.model->equations.size(), 1u);
  const auto &eq = r.model->equations[0];
  EXPECT_TRUE(eq.isIf);
  ASSERT_EQ(eq.branches.size(), 2u);
  // 首分支带条件（b），末分支为 else（condition 为空）。
  EXPECT_NE(eq.branches[0].condition.get(), nullptr);
  EXPECT_EQ(eq.branches[1].condition.get(), nullptr);
  for (const auto &b : eq.branches) {
    ASSERT_EQ(b.equations.size(), 1u);
    EXPECT_EQ(b.equations[0].lhs.expr->kind, ast::ExprKind::Ident);
    EXPECT_EQ(b.equations[0].lhs.expr->token.lexeme, "v");
  }
}

TEST(ParserConditional, ParsesDerBranchForm) {
  const auto r = parseSrc(R"(model M
  Real s(start = 1, fixed = true);
equation
  if s < 1 then
    der(s) = 1;
  else
    der(s) = 2;
  end if;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.diags.all().empty());
  ASSERT_TRUE(r.model.has_value());
  ASSERT_EQ(r.model->equations.size(), 1u);
  const auto &eq = r.model->equations[0];
  EXPECT_TRUE(eq.isIf);
  ASSERT_EQ(eq.branches.size(), 2u);
  for (const auto &b : eq.branches) {
    ASSERT_EQ(b.equations.size(), 1u);
    EXPECT_TRUE(b.equations[0].lhs.isDer);
    EXPECT_EQ(b.equations[0].lhs.targetTok.lexeme, "s");
  }
}

TEST(ParserConditional, ParsesIfKeywordBeforeAnnotationIsHandled) {
  // 回归护栏：函数名/标识符含 if_ 前缀不影响 if 表达式分派（关键字为专名）。
  const auto r = parseSrc(R"(model M
  Real ifx(start = 1, fixed = true);
equation
  der(ifx) = 0;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  EXPECT_TRUE(r.diags.all().empty());
}

// ---- 条件构造解析·elseif 链（spec 003 T019）----

TEST(ParserConditional, ParsesElseifChain) {
  const auto r = parseSrc(R"(model M
  Real v;
  Real s(start = 0, fixed = true);
  Boolean b;
equation
  if s < 1 then
    v = 1;
  elseif b then
    v = 2;
  elseif s < 3 then
    v = 3;
  else
    v = 4;
  end if;
  annotation(experiment(StopTime = 4, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.diags.all().empty());
  ASSERT_TRUE(r.model.has_value());
  ASSERT_EQ(r.model->equations.size(), 1u);
  const auto &eq = r.model->equations[0];
  EXPECT_TRUE(eq.isIf);
  ASSERT_EQ(eq.branches.size(), 4u);
  // 前三个分支带条件，末分支为 else（condition 为空）。
  EXPECT_NE(eq.branches[0].condition.get(), nullptr);
  EXPECT_NE(eq.branches[1].condition.get(), nullptr);
  EXPECT_NE(eq.branches[2].condition.get(), nullptr);
  EXPECT_EQ(eq.branches[3].condition.get(), nullptr);
  for (const auto &b : eq.branches) {
    ASSERT_EQ(b.equations.size(), 1u);
    EXPECT_EQ(b.equations[0].lhs.expr->kind, ast::ExprKind::Ident);
    EXPECT_EQ(b.equations[0].lhs.expr->token.lexeme, "v");
  }
}

TEST(ParserConditional, ParsesNestedTernaryExpression) {
  const auto r = parseSrc(R"(model M
  Real w;
  Real s(start = 2, fixed = true);
equation
  w = if s < 2 then (if s < 1 then 1 else 2) else 3;
  der(s) = -1;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.diags.all().empty());
  ASSERT_TRUE(r.model.has_value());
  ASSERT_EQ(r.model->equations.size(), 2u);
  const auto &rhs = r.model->equations[0].rhs;
  ASSERT_NE(rhs.expr.get(), nullptr);
  EXPECT_EQ(rhs.expr->kind, ast::ExprKind::If);
  // thenExpr 应为括号内嵌套的 If（内层三元 1/2）。
  ASSERT_NE(rhs.expr->thenExpr.get(), nullptr);
  EXPECT_EQ(rhs.expr->thenExpr->kind, ast::ExprKind::If);
  ASSERT_NE(rhs.expr->elseExpr.get(), nullptr);
  EXPECT_EQ(rhs.expr->elseExpr->kind, ast::ExprKind::NumLit); // 3
}

TEST(ParserConditional, ParsesNestedIfEquation) {
  const auto r = parseSrc(R"(model M
  Real v;
  Boolean b;
  Boolean c;
equation
  if b then
    if c then
      v = 1;
    else
      v = 2;
    end if;
  else
    v = 3;
  end if;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  ASSERT_TRUE(r.diags.all().empty());
  ASSERT_TRUE(r.model.has_value());
  ASSERT_EQ(r.model->equations.size(), 1u);
  const auto &eq = r.model->equations[0];
  EXPECT_TRUE(eq.isIf);
  ASSERT_EQ(eq.branches.size(), 2u);
  // 首分支内含一条嵌套的条件方程。
  const auto &inner = eq.branches[0].equations[0];
  EXPECT_TRUE(inner.isIf);
  ASSERT_EQ(inner.branches.size(), 2u);
  EXPECT_EQ(inner.branches[0].equations[0].lhs.expr->token.lexeme, "v");
  // 末分支（else）为普通赋值。
  EXPECT_FALSE(eq.branches[1].equations[0].isIf);
}

TEST(ParserConditional, ParsesIfExprMissingThenReportsLocatedError) {
  // 缺 then：parseIfExpression 的 expect(then) 报定位错误（"期望 then"）。
  const auto r = parseSrc(R"(model M
  Real y;
  Real x(start = 1, fixed = true);
equation
  y = if x > 0 1 else 2;
  der(x) = 0;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  // 期望 then（第 5 行，"1" 处）；解析应报错并定位到该行。
  EXPECT_TRUE(hasCode(r.diags, Code::ExprUnsupported))
      << "缺少 then 应产生解析错误";
  bool found = false;
  for (const auto &d : r.diags.all()) {
    if (d.code == Code::ExprUnsupported && d.location.line == 5) {
      found = true;
      EXPECT_NE(d.message.find("then"), std::string::npos);
    }
  }
  EXPECT_TRUE(found) << "then 错误应定位在第 5 行末尾";
}

TEST(ParserConditional, ParsesIfExprMissingElseReportsLocatedError) {
  // 缺 else：expect(else) 在表达式边界外报错。
  const auto r = parseSrc(R"(model M
  Boolean b;
  Real x(start = 1, fixed = true);
equation
  x = if b then 1 + ;
  der(x) = 0;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  EXPECT_TRUE(hasCode(r.diags, Code::ExprUnsupported))
      << "缺少 else/then 应产生解析错误";
}

TEST(ParserConditional, ParsesIfEqMissingEndIfReportsLocatedError) {
  // 缺 end if：else 分支结束后未遇见 elseif/else/end，分支列表不收敛 → 报错。
  const auto r = parseSrc(R"(model M
  Real v;
  Boolean b;
  annotation(experiment(StopTime = 2, Interval = 0.5));
equation
  if b then
    v = 1;
  else
    v = 2;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  EXPECT_TRUE(hasCode(r.diags, Code::ExprUnsupported))
      << "缺少 end if 应产生解析错误（分支不收敛处）";
}

TEST(ParserConditional, ParsesIfExprNonExpressionAfterIf) {
  // if 后跟非表达式（`der(` 紧接着不合法起点）：不能静默接受，需报错。
  const auto r = parseSrc(R"(model M
  Real x(start = 1, fixed = true);
equation
  x = if ) then 1 else 2;
  der(x) = 0;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)");
  EXPECT_TRUE(hasCode(r.diags, Code::ExprUnsupported));
}

// ---- 嵌套深度护栏（spec 003 T032）----

namespace {

// 生成 depth 层嵌套的条件表达式（then 分支内嵌）。
std::string nestedIfExpr(int depth) {
  std::string e = "1";
  for (int i = 0; i < depth; ++i)
    e = "if c then (" + e + ") else 0";
  return e;
}

} // namespace

TEST(ParserConditional, RejectsExcessiveIfNestingWithoutCrash) {
  // kMaxIfDepth=1024；构造 1100 层嵌套，应在超限处报清晰错误而非栈溢出。
  const std::string src = "model M\n  Boolean c;\n  Real y;\n  Real x(start = 1, fixed = true);\n"
                          "equation\n  y = " +
                          nestedIfExpr(1100) + ";\n  der(x) = 0;\n" +
                          "  annotation(experiment(StopTime = 2, Interval = 0.5));\nend M;\n";
  const auto r = parseSrc(src);
  EXPECT_TRUE(hasCode(r.diags, Code::ExprUnsupported)) << "超深嵌套应报解析错误";
  bool sawDepth = false;
  for (const auto &d : r.diags.all()) {
    if (d.code == Code::ExprUnsupported && d.message.find("深度") != std::string::npos)
      sawDepth = true;
  }
  EXPECT_TRUE(sawDepth) << "错误信息应指明嵌套过深";
}

TEST(ParserConditional, AcceptsReasonableIfNesting) {
  // 远低于护栏的嵌套（如 200 层）应正常解析。
  const std::string src = "model M\n  Boolean c;\n  Real y;\n  Real x(start = 1, fixed = true);\n"
                          "equation\n  y = " +
                          nestedIfExpr(200) + ";\n  der(x) = 0;\n" +
                          "  annotation(experiment(StopTime = 2, Interval = 0.5));\nend M;\n";
  const auto r = parseSrc(src);
  EXPECT_TRUE(r.diags.all().empty()) << "200 层嵌套应无诊断";
  ASSERT_TRUE(r.model.has_value());
}

} // namespace
