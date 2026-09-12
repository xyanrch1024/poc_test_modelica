#include "parser/parser.h"

#include <unordered_set>

namespace mcdc {

namespace {

// 首期明确不支持的构造关键字（contracts/subset-grammar.md "明确不支持"清单）。
const std::unordered_set<std::string> &kUnsupportedKeywords() {
  static const std::unordered_set<std::string> kSet = {
      "package", "import", "algorithm", "when",
      "connect", "reinit", "record",    "function", "each",
  };
  return kSet;
}

bool isDeclarationKeyword(const Token &t) {
  if (t.kind != TokKind::Keyword)
    return false;
  return t.lexeme == "constant" || t.lexeme == "parameter" || t.lexeme == "Real" ||
         t.lexeme == "Integer" || t.lexeme == "Boolean";
}

bool parseNumberToken(const Token &tok, double *out) {
  try {
    size_t consumed = 0;
    const double v = std::stod(tok.lexeme, &consumed);
    if (consumed != tok.lexeme.size())
      return false;
    *out = v;
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace

Parser::Parser(std::vector<Token> tokens, DiagnosticCollector &diags)
    : tokens_(std::move(tokens)), diags_(diags) {}

bool Parser::atEnd() const {
  return peek().kind == TokKind::End;
}

const Token &Parser::peek(size_t offset) const {
  const size_t idx = pos_ + offset < tokens_.size() ? pos_ + offset : tokens_.size() - 1;
  return tokens_[idx];
}

const Token &Parser::advance() {
  if (!atEnd())
    ++pos_;
  return tokens_[pos_ - 1];
}

bool Parser::check(TokKind kind, const std::string &lexeme) const {
  const Token &t = peek();
  return t.kind == kind && (lexeme.empty() || t.lexeme == lexeme);
}

bool Parser::match(TokKind kind, const std::string &lexeme) {
  if (check(kind, lexeme)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::expect(TokKind kind, const std::string &lexeme, const char *what) {
  if (match(kind, lexeme))
    return true;
  const Token &t = peek();
  diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                  std::string("期望 ") + what + "，实际为 \"" + t.lexeme + "\"");
  return false;
}

void Parser::reportUnsupported(const Token &tok) {
  diags_.addError(Location{"", tok.line, tok.col}, Code::UnsupportedConstruct,
                  "构造 \"" + tok.lexeme + "\" 不在首期支持子集内");
}

void Parser::synchronizeStatement() {
  while (!atEnd()) {
    if (peek().kind == TokKind::Op && peek().lexeme == ";") {
      advance();
      return;
    }
    // 语句边界关键字也可作为同步点。
    if (peek().kind == TokKind::Keyword &&
        (peek().lexeme == "end" || peek().lexeme == "equation" || isDeclarationKeyword(peek()))) {
      return;
    }
    advance();
  }
}

std::optional<ast::Model> Parser::parseFile() {
  // 空文件 / 仅注释 → MC0001
  if (atEnd()) {
    const Token &t = peek();
    diags_.addError(Location{"", t.line, t.col}, Code::StructureEmptyFile,
                    "文件为空或仅含注释，未发现可翻译的模型");
    return std::nullopt;
  }

  ast::Model model;
  model.modelTok = advance(); // 'model'
  if (model.modelTok.kind != TokKind::Keyword || model.modelTok.lexeme != "model") {
    diags_.addError(Location{"", model.modelTok.line, model.modelTok.col}, Code::ExprUnsupported,
                    "文件必须以模型声明 \"model\" 开始");
    synchronizeStatement();
    return std::nullopt;
  }
  if (!expect(TokKind::Ident, "", "模型名"))
    return std::nullopt;
  model.nameTok = tokens_[pos_ - 1];

  match(TokKind::String, ""); // 可选描述字符串

  bool inEquationSection = false;

  while (!atEnd()) {
    const Token &t = peek();

    if (t.kind == TokKind::Keyword && kUnsupportedKeywords().count(t.lexeme)) {
      reportUnsupported(t);
      synchronizeStatement();
      continue;
    }
    if (t.kind == TokKind::Keyword && t.lexeme == "annotation") {
      auto exp = parseExperimentAnnotation();
      if (exp && !inEquationSection) {
        // 注解出现在 equation 之前：语法上允许，但语义要求在段尾；此处仍接受。
      }
      if (exp) {
        if (model.experiment) {
          diags_.addError(Location{"", exp->annTok.line, exp->annTok.col}, Code::ExperimentInvalid,
                          "重复的实验设置注释");
        } else {
          model.experiment = *exp;
        }
      }
      continue;
    }
    if (t.kind == TokKind::Keyword && t.lexeme == "equation") {
      advance();
      inEquationSection = true;
      continue;
    }
    if (!inEquationSection && t.kind == TokKind::Keyword && t.lexeme == "der") {
      // der 出现在方程区之外：非法位置。
      diags_.addError(Location{"", t.line, t.col}, Code::ExprBadDerPlacement,
                      "der(...) 只能出现在方程左侧");
      synchronizeStatement();
      continue;
    }
    if (t.kind == TokKind::Keyword && t.lexeme == "end") {
      break;
    }

    if (isDeclarationKeyword(t)) {
      if (inEquationSection) {
        diags_.addError(Location{"", t.line, t.col}, Code::StructureMultipleModels,
                        "equation 段内不允许新的声明");
        synchronizeStatement();
        continue;
      }
      auto comp = parseComponent();
      if (comp)
        model.components.push_back(std::move(*comp));
      continue;
    }

    if (inEquationSection && (t.kind == TokKind::Ident || t.kind == TokKind::Keyword ||
                              t.kind == TokKind::Int || t.kind == TokKind::Real)) {
      auto eq = parseEquation();
      if (eq)
        model.equations.push_back(std::move(*eq));
      continue;
    }

    if (t.kind == TokKind::Keyword && t.lexeme == "model") {
      diags_.addError(Location{"", t.line, t.col}, Code::StructureMultipleModels,
                      "一个文件仅允许包含一个模型（FR-004）");
      reportUnsupported(t);
      synchronizeStatement();
      continue;
    }

    // 无法归类的 token：报告并跳过该"语句"。
    diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                    "无法解析的记号 \"" + t.lexeme + "\"");
    advance();
    synchronizeStatement();
  }

  if (!match(TokKind::Keyword, "end")) {
    const Token &t = peek();
    diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported, "缺少模型结束标记 end");
    return std::nullopt;
  }
  expect(TokKind::Ident, model.nameTok.lexeme, "与模型名一致的结束标识");
  match(TokKind::Op, ";");

  // 单文件仅允许一个模型（FR-004）：结尾之后若再次出现模型声明则报告 MC0002。
  if (!atEnd()) {
    const Token &t = peek();
    if (t.kind == TokKind::Keyword && t.lexeme == "model") {
      diags_.addError(Location{"", t.line, t.col}, Code::StructureMultipleModels,
                      "一个文件仅允许包含一个模型（FR-004）");
    } else {
      diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                      "模型结束后存在多余内容 \"" + t.lexeme + "\"");
    }
  }

  return model;
}

std::optional<ast::Component> Parser::parseComponent() {
  ast::Component comp;
  comp.kindTok = advance();

  const bool kindIsType = comp.kindTok.kind == TokKind::Keyword &&
                          (comp.kindTok.lexeme == "Real" || comp.kindTok.lexeme == "Integer" ||
                           comp.kindTok.lexeme == "Boolean");

  if (kindIsType) {
    // 形如 "Real x;" —— 同一 token 兼任类型。
    comp.typeTok = comp.kindTok;
    comp.kind = ast::Component::Kind::Variable;
    if (comp.typeTok.lexeme == "Integer")
      comp.type = ast::Component::DeclType::Integer;
    if (comp.typeTok.lexeme == "Boolean")
      comp.type = ast::Component::DeclType::Boolean;
  } else {
    if (comp.kindTok.lexeme == "constant")
      comp.kind = ast::Component::Kind::Constant;
    if (comp.kindTok.lexeme == "parameter")
      comp.kind = ast::Component::Kind::Parameter;
    if (!check(TokKind::Keyword, "Real") && !check(TokKind::Keyword, "Integer") &&
        !check(TokKind::Keyword, "Boolean")) {
      const Token &t = peek();
      diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                      "声明需要类型 Real/Integer/Boolean");
      synchronizeStatement();
      return std::nullopt;
    }
    comp.typeTok = advance();
    if (comp.typeTok.lexeme == "Integer")
      comp.type = ast::Component::DeclType::Integer;
    if (comp.typeTok.lexeme == "Boolean")
      comp.type = ast::Component::DeclType::Boolean;
  }

  if (!expect(TokKind::Ident, "", "变量名")) {
    synchronizeStatement();
    return std::nullopt;
  }
  comp.nameTok = tokens_[pos_ - 1];

  // 属性表 (start=..., fixed=..., unit="...")
  if (match(TokKind::Op, "(")) {
    while (!atEnd() && !check(TokKind::Op, ")")) {
      if (!check(TokKind::Keyword, "start") && !check(TokKind::Keyword, "fixed") &&
          !check(TokKind::Keyword, "unit")) {
        const Token &bad = peek();
        diags_.addError(Location{"", bad.line, bad.col}, Code::ExprUnsupported,
                        "不支持的属性 \"" + bad.lexeme + "\"（仅 start/fixed/unit）");
        synchronizeStatement();
        return std::nullopt;
      }
      const Token attrTok = advance();
      if (!expect(TokKind::Op, "=", "=")) {
        synchronizeStatement();
        return std::nullopt;
      }
      if (attrTok.lexeme == "unit") {
        if (!match(TokKind::String, "")) {
          const Token &t = peek();
          diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                          "unit 属性需要字符串值");
          synchronizeStatement();
          return std::nullopt;
        }
      } else {
        auto valueExpr = parseExpression();
        if (!valueExpr) {
          synchronizeStatement();
          return std::nullopt;
        }
        if (attrTok.lexeme == "start") {
          comp.start = std::move(valueExpr);
          comp.hasStart = true;
        } else {
          comp.fixed = std::move(valueExpr);
          comp.hasFixed = true;
        }
      }
      if (!match(TokKind::Op, ","))
        break;
    }
    if (!expect(TokKind::Op, ")", ")")) {
      synchronizeStatement();
      return std::nullopt;
    }
  }

  if (match(TokKind::Op, "=")) {
    auto valueExpr = parseExpression();
    if (!valueExpr) {
      synchronizeStatement();
      return std::nullopt;
    }
    comp.value = std::move(valueExpr);
  }

  match(TokKind::String, ""); // 可选描述
  if (!expect(TokKind::Op, ";", "\";\"")) {
    synchronizeStatement();
    return std::nullopt;
  }
  return comp;
}

ast::EquationSide Parser::parseEquationSide() {
  ast::EquationSide side;
  // der( IDENT )
  if (check(TokKind::Keyword, "der") && peek(1).kind == TokKind::Op && peek(1).lexeme == "(") {
    side.isDer = true;
    side.derToken = advance();
    advance(); // (
    if (!expect(TokKind::Ident, "", "求导目标变量")) {
      side.expr = nullptr;
      return side;
    }
    side.targetTok = tokens_[pos_ - 1];
    expect(TokKind::Op, ")", ")");
    return side;
  }
  side.expr = parseExpression();
  return side;
}

std::optional<ast::Equation> Parser::parseEquation() {
  // 条件方程形态：if <cond> then ... elseif ... else ... end if;
  if (check(TokKind::Keyword, "if"))
    return parseIfEquation();

  ast::Equation eq;
  eq.lhs = parseEquationSide();
  if (!eq.lhs.isDer && !eq.lhs.expr) {
    synchronizeStatement();
    return std::nullopt;
  }
  if (!expect(TokKind::Op, "=", "\"=\"")) {
    synchronizeStatement();
    return std::nullopt;
  }
  eq.rhs = parseEquationSide();
  if (!eq.rhs.isDer && !eq.rhs.expr) {
    synchronizeStatement();
    return std::nullopt;
  }
  match(TokKind::String, "");
  if (!expect(TokKind::Op, ";", "\";\"")) {
    synchronizeStatement();
    return std::nullopt;
  }
  return eq;
}

std::optional<ast::Equation> Parser::parseIfEquation() {
  if (ifDepth_ >= kMaxIfDepth) {
    const Token &t = peek();
    diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                    "if 方程嵌套超过安全深度");
    synchronizeStatement();
    return std::nullopt;
  }
  ++ifDepth_;
  struct DepthGuard {
    int &depth;
    ~DepthGuard() { --depth; }
  } guard{ifDepth_};

  ast::Equation eq;
  eq.isIf = true;
  eq.ifTok = advance(); // 'if'

  // 分支 0：if <cond> then
  {
    auto cond = parseExpression();
    if (!cond) {
      synchronizeStatement();
      return std::nullopt;
    }
    if (!expect(TokKind::Keyword, "then", "then")) {
      synchronizeStatement();
      return std::nullopt;
    }
    ast::IfBranch branch;
    branch.condition = std::move(cond);
    while (!atEnd() && !atIfBranchEnd()) {
      auto sub = parseEquation();
      if (!sub) {
        synchronizeStatement();
        return std::nullopt;
      }
      branch.equations.push_back(std::move(*sub));
    }
    eq.branches.push_back(std::move(branch));
  }

  while (!atEnd()) {
    if (match(TokKind::Keyword, "elseif")) {
      ast::IfBranch branch;
      branch.condition = parseExpression();
      if (!branch.condition) {
        synchronizeStatement();
        return std::nullopt;
      }
      if (!expect(TokKind::Keyword, "then", "then")) {
        synchronizeStatement();
        return std::nullopt;
      }
      while (!atEnd() && !atIfBranchEnd()) {
        auto sub = parseEquation();
        if (!sub) {
          synchronizeStatement();
          return std::nullopt;
        }
        branch.equations.push_back(std::move(*sub));
      }
      eq.branches.push_back(std::move(branch));
      continue;
    }
    if (match(TokKind::Keyword, "else")) {
      ast::IfBranch branch; // condition 为空 → else 分支
      while (!atEnd() && !atIfBranchEnd()) {
        auto sub = parseEquation();
        if (!sub) {
          synchronizeStatement();
          return std::nullopt;
        }
        branch.equations.push_back(std::move(*sub));
      }
      eq.branches.push_back(std::move(branch));
      continue;
    }
    break;
  }

  if (!expect(TokKind::Keyword, "end", "end")) {
    synchronizeStatement();
    return std::nullopt;
  }
  if (!expect(TokKind::Keyword, "if", "\"if\"")) {
    synchronizeStatement();
    return std::nullopt;
  }
  match(TokKind::String, "");
  if (!expect(TokKind::Op, ";", "\";\"")) {
    synchronizeStatement();
    return std::nullopt;
  }
  return eq;
}

// 分支方程列表的终止符：elseif / else / end（都不属于正常方程起始）。
bool Parser::atIfBranchEnd() const {
  if (atEnd())
    return true;
  if (peek().kind != TokKind::Keyword)
    return false;
  const std::string &l = peek().lexeme;
  return l == "elseif" || l == "else" || l == "end";
}

std::optional<ast::Experiment> Parser::parseExperimentAnnotation() {
  ast::Experiment exp;
  exp.annTok = advance(); // annotation
  if (!expect(TokKind::Op, "(", "("))
    return std::nullopt;
  if (!expect(TokKind::Keyword, "experiment", "experiment")) {
    reportUnsupported(peek());
    synchronizeStatement();
    return std::nullopt;
  }
  if (!expect(TokKind::Op, "(", "("))
    return std::nullopt;

  while (!atEnd() && !check(TokKind::Op, ")")) {
    // 实验选项名是普通标识符（非语言关键字），按 lexeme 匹配。
    const bool isOptionKey = (peek().kind == TokKind::Ident || peek().kind == TokKind::Keyword) &&
                             (peek().lexeme == "StartTime" || peek().lexeme == "StopTime" ||
                              peek().lexeme == "Interval" || peek().lexeme == "Tolerance");
    if (!isOptionKey) {
      const Token &bad = peek();
      diags_.addError(Location{"", bad.line, bad.col}, Code::ExperimentInvalid,
                      "不支持的实验选项 \"" + bad.lexeme + "\"");
      synchronizeStatement();
      return std::nullopt;
    }
    const Token keyTok = advance();
    expect(TokKind::Op, "=", "=");
    const Token valueTok = advance();
    double number = 0.0;
    if ((valueTok.kind != TokKind::Real && valueTok.kind != TokKind::Int) ||
        !parseNumberToken(valueTok, &number)) {
      diags_.addError(Location{"", valueTok.line, valueTok.col}, Code::ExperimentInvalid,
                      "实验选项需要数值");
      synchronizeStatement();
      return std::nullopt;
    }
    if (keyTok.lexeme == "StartTime") {
      exp.startTime = number;
    } else if (keyTok.lexeme == "StopTime") {
      exp.stopTime = number;
      exp.hasStopTime = true;
    } else if (keyTok.lexeme == "Interval") {
      exp.interval = number;
      exp.hasInterval = true;
    }
    if (!match(TokKind::Op, ","))
      break;
  }
  expect(TokKind::Op, ")", ")");
  expect(TokKind::Op, ")", ")");
  expect(TokKind::Op, ";", "\";\"");
  return exp;
}

ast::ExprPtr Parser::parseExpression() {
  return parseLogicalOr();
}

ast::ExprPtr Parser::parseLogicalOr() {
  auto lhs = parseLogicalAnd();
  if (!lhs)
    return nullptr;
  while (check(TokKind::Keyword, "or")) {
    const Token opTok = advance();
    auto rhs = parseLogicalAnd();
    if (!rhs)
      return nullptr;
    lhs = ast::Expr::binary(opTok, "or", std::move(lhs), std::move(rhs));
  }
  return lhs;
}

ast::ExprPtr Parser::parseLogicalAnd() {
  auto lhs = parseNot();
  if (!lhs)
    return nullptr;
  while (check(TokKind::Keyword, "and")) {
    const Token opTok = advance();
    auto rhs = parseNot();
    if (!rhs)
      return nullptr;
    lhs = ast::Expr::binary(opTok, "and", std::move(lhs), std::move(rhs));
  }
  return lhs;
}

ast::ExprPtr Parser::parseNot() {
  if (check(TokKind::Keyword, "not")) {
    const Token opTok = advance();
    auto operand = parseNot();
    if (!operand)
      return nullptr;
    return ast::Expr::unary(opTok, "not", std::move(operand));
  }
  return parseComparison();
}

ast::ExprPtr Parser::parseComparison() {
  auto lhs = parseAdditive();
  if (!lhs)
    return nullptr;
  if (check(TokKind::Op, "<") || check(TokKind::Op, "<=") || check(TokKind::Op, ">") ||
      check(TokKind::Op, ">=") || check(TokKind::Op, "==") || check(TokKind::Op, "<>")) {
    const Token opTok = advance();
    auto rhs = parseAdditive();
    if (!rhs)
      return nullptr;
    return ast::Expr::binary(opTok, opTok.lexeme, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

ast::ExprPtr Parser::parseAdditive() {
  auto lhs = parseMultiplicative();
  if (!lhs)
    return nullptr;
  while (check(TokKind::Op, "+") || check(TokKind::Op, "-")) {
    const Token opTok = advance();
    auto rhs = parseMultiplicative();
    if (!rhs)
      return nullptr;
    lhs = ast::Expr::binary(opTok, opTok.lexeme, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

ast::ExprPtr Parser::parseMultiplicative() {
  auto lhs = parseUnary();
  if (!lhs)
    return nullptr;
  while (check(TokKind::Op, "*") || check(TokKind::Op, "/")) {
    const Token opTok = advance();
    auto rhs = parseUnary();
    if (!rhs)
      return nullptr;
    lhs = ast::Expr::binary(opTok, opTok.lexeme, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

ast::ExprPtr Parser::parseUnary() {
  if (check(TokKind::Op, "+") || check(TokKind::Op, "-")) {
    const Token opTok = advance();
    auto operand = parseUnary();
    if (!operand)
      return nullptr;
    if (opTok.lexeme == "+")
      return operand; // 一元正号无操作
    return ast::Expr::unary(opTok, "-", std::move(operand));
  }
  return parsePrimary();
}

ast::ExprPtr Parser::parsePrimary() {
  const Token &t = peek();
  if (check(TokKind::Keyword, "if"))
    return parseIfExpression();
  if (t.kind == TokKind::Real || t.kind == TokKind::Int) {
    Token tok = advance();
    double value = 0.0;
    parseNumberToken(tok, &value);
    return ast::Expr::num(std::move(tok), value);
  }
  if (check(TokKind::Keyword, "true") || check(TokKind::Keyword, "false")) {
    Token tok = advance();
    return ast::Expr::boolean(std::move(tok), tok.lexeme == "true");
  }
  if (check(TokKind::Keyword, "der")) {
    // 表达式内部的 der —— 非法位置；按 der(Ident) 解析后交由语义检查报 MC0203。
    auto e = parseEquationSide();
    if (e.isDer) {
      return ast::Expr::der(e.derToken, e.targetTok);
    }
    return nullptr;
  }
  if (t.kind == TokKind::Ident) {
    Token nameTok = advance();
    if (check(TokKind::Op, "(")) {
      advance();
      std::vector<ast::ExprPtr> args;
      if (!check(TokKind::Op, ")")) {
        while (true) {
          auto arg = parseExpression();
          if (!arg)
            return nullptr;
          args.push_back(std::move(arg));
          if (!match(TokKind::Op, ","))
            break;
        }
      }
      expect(TokKind::Op, ")", ")");
      return ast::Expr::call(std::move(nameTok), std::move(args));
    }
    return ast::Expr::ident(std::move(nameTok));
  }
  if (match(TokKind::Op, "(")) {
    auto inner = parseExpression();
    if (!inner)
      return nullptr;
    expect(TokKind::Op, ")", ")");
    return inner;
  }
  diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                  "无法解析的表达式起点 \"" + t.lexeme + "\"");
  return nullptr;
}

ast::ExprPtr Parser::parseIfExpression() {
  if (ifDepth_ >= kMaxIfDepth) {
    const Token &t = peek();
    diags_.addError(Location{"", t.line, t.col}, Code::ExprUnsupported,
                    "if 表达式嵌套超过安全深度");
    return nullptr;
  }
  ++ifDepth_;
  struct DepthGuard {
    int &depth;
    ~DepthGuard() { --depth; }
  } guard{ifDepth_};

  const Token ifTok = advance(); // 'if'
  auto cond = parseExpression();
  if (!cond)
    return nullptr;
  if (!expect(TokKind::Keyword, "then", "then"))
    return nullptr;
  auto thenExpr = parseExpression();
  if (!thenExpr)
    return nullptr;
  if (!expect(TokKind::Keyword, "else", "else"))
    return nullptr;
  auto elseExpr = parseExpression();
  if (!elseExpr)
    return nullptr;
  return ast::Expr::if_(ifTok, std::move(cond), std::move(thenExpr), std::move(elseExpr));
}

} // namespace mcdc
