#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "lexer/lexer.h"

namespace {

using namespace mcdc;

std::vector<Token> lex(const std::string &src, DiagnosticCollector *diags = nullptr) {
  DiagnosticCollector fallback;
  auto tokens = Lexer(src).tokenize(diags != nullptr ? *diags : fallback);
  return tokens;
}

TEST(LexerBasics, TracksLineAndColumnFromOne) {
  const auto t = lex("model Cooling\n  Real x;\n");
  ASSERT_GE(t.size(), 3u);
  EXPECT_EQ(t[0].kind, TokKind::Keyword);
  EXPECT_EQ(t[0].lexeme, "model");
  EXPECT_EQ(t[0].line, 1);
  EXPECT_EQ(t[0].col, 1);

  EXPECT_EQ(t[1].kind, TokKind::Ident);
  EXPECT_EQ(t[1].lexeme, "Cooling");
  EXPECT_EQ(t[1].line, 1);
  EXPECT_EQ(t[1].col, 7);

  // 第二行缩进两个空格后的 Real
  bool found = false;
  for (const auto &tok : t) {
    if (tok.lexeme == "Real") {
      EXPECT_EQ(tok.line, 2);
      EXPECT_EQ(tok.col, 3);
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST(LexerBasics, ClassifiesNumbersStringsOperators) {
  const auto t = lex("1.5e-2 42 \"text\" <= == <> + * ( ) , ; =");
  std::vector<std::pair<TokKind, std::string>> want{
      {TokKind::Real, "1.5e-2"}, {TokKind::Int, "42"}, {TokKind::String, "text"},
      {TokKind::Op, "<="},       {TokKind::Op, "=="},  {TokKind::Op, "<>"},
      {TokKind::Op, "+"},        {TokKind::Op, "*"},   {TokKind::Op, "("},
      {TokKind::Op, ")"},        {TokKind::Op, ","},   {TokKind::Op, ";"},
      {TokKind::Op, "="},
  };
  ASSERT_EQ(t.size(), want.size() + 1); // + End
  for (size_t i = 0; i < want.size(); ++i) {
    EXPECT_EQ(t[i].kind, want[i].first) << "index " << i;
    EXPECT_EQ(t[i].lexeme, want[i].second) << "index " << i;
  }
}

TEST(LexerBasics, SeparatesKeywordsFromIdentifiers) {
  const auto t = lex("model ender derx equation");
  ASSERT_EQ(t.size(), 5u);
  EXPECT_EQ(t[0].kind, TokKind::Keyword);
  EXPECT_EQ(t[1].kind, TokKind::Ident); // "ender" 不是关键字
  EXPECT_EQ(t[2].kind, TokKind::Ident); // "derx" 不是关键字
  EXPECT_EQ(t[3].kind, TokKind::Keyword);
}

TEST(LexerComments, SkipsLineAndBlockComments) {
  const auto t = lex("// 全行注释\na /* 块\n注释 */ b");
  ASSERT_EQ(t.size(), 3u);
  EXPECT_EQ(t[0].lexeme, "a");
  EXPECT_EQ(t[0].line, 2);
  EXPECT_EQ(t[1].lexeme, "b");
  EXPECT_EQ(t[1].line, 3);
}

TEST(LexerErrors, ReportsIllegalCharacterWithPosition) {
  DiagnosticCollector diags;
  const auto t = lex("x # y", &diags);
  EXPECT_EQ(diags.all().size(), 1u);
  EXPECT_TRUE(diags.hasErrors());
  EXPECT_EQ(diags.all()[0].location.line, 1);
  EXPECT_EQ(diags.all()[0].location.col, 3);
  EXPECT_FALSE(t.empty());
}

TEST(LexerErrors, ReportsUnterminatedString) {
  DiagnosticCollector diags;
  lex("x = \"oops", &diags);
  EXPECT_EQ(diags.all().size(), 1u);
  EXPECT_EQ(diags.all()[0].code, Code::ExprUnsupported);
}

} // namespace
