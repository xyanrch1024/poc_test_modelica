#include "codegen/plan.h"

#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace mcdc {

namespace {

const std::unordered_set<std::string> &kCppKeywords() {
  static const std::unordered_set<std::string> kSet = {
      "alignas",      "alignof",   "and",           "and_eq",
      "asm",          "auto",      "bitand",        "bitor",
      "bool",         "break",     "case",          "catch",
      "char",         "char8_t",   "class",         "compl",
      "concept",      "const",     "consteval",     "constexpr",
      "constinit",    "const_cast","continue",      "co_await",
      "co_return",    "co_yield",  "decltype",      "default",
      "delete",       "do",        "double",        "dynamic_cast",
      "else",         "enum",      "explicit",      "export",
      "extern",       "false",     "float",         "for",
      "friend",       "goto",      "if",            "inline",
      "int",          "long",      "mutable",       "namespace",
      "new",          "noexcept",  "not",           "not_eq",
      "nullptr",      "operator",  "or",            "or_eq",
      "private",      "protected", "public",        "register",
      "reinterpret_cast", "requires", "return",     "short",
      "signed",       "sizeof",    "static",        "static_assert",
      "static_cast",  "struct",    "switch",        "template",
      "this",         "thread_local", "throw",      "true",
      "try",          "typedef",   "typeid",        "typename",
      "union",        "unsigned",  "using",         "virtual",
      "void",         "volatile",  "wchar_t",       "while",
      "xor",          "xor_eq",
  };
  return kSet;
}

std::string formatReal(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  return buf;
}

std::string literalFromToken(const Token &tok, double value) {
  if (tok.kind == TokKind::Int)
    return tok.lexeme;
  return formatReal(value);
}

PlanVar makeVar(const std::string &name, ast::Component::DeclType type) {
  return PlanVar{name, mapToCppIdentifier(name), type};
}

} // namespace

std::string mapToCppIdentifier(const std::string &name) {
  if (kCppKeywords().count(name))
    return name + "_";
  return name;
}

std::optional<double> evalConstExpr(const ast::Expr &expr, const ast::Model &model,
                                    const SymbolTable &table, std::set<std::string> *visiting,
                                    DiagnosticCollector &diags) {
  switch (expr.kind) {
  case ast::ExprKind::NumLit:
    return expr.numValue;
  case ast::ExprKind::BoolLit:
    return expr.boolValue ? 1.0 : 0.0;
  case ast::ExprKind::Der:
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                   "初始化/属性值必须是数值常量表达式");
    return std::nullopt;
  case ast::ExprKind::Ident: {
    const std::string &name = expr.token.lexeme;
    if (visiting->count(name)) {
      diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                     "常量表达式存在循环引用 \"" + name + "\"");
      return std::nullopt;
    }
    for (const auto &comp : model.components) {
      if (comp.nameTok.lexeme != name)
        continue;
      if (comp.kind == ast::Component::Kind::Variable || !comp.value) {
        diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                       "常量表达式中引用了非常量 \"" + name + "\"");
        return std::nullopt;
      }
      visiting->insert(name);
      auto value = evalConstExpr(*comp.value, model, table, visiting, diags);
      visiting->erase(name);
      return value;
    }
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::SymbolUndeclared,
                   "使用未声明标识符 \"" + name + "\"");
    return std::nullopt;
  }
  case ast::ExprKind::Unary: {
    auto operand = evalConstExpr(*expr.lhs, model, table, visiting, diags);
    if (!operand)
      return std::nullopt;
    return -*operand;
  }
  case ast::ExprKind::Binary: {
    auto l = evalConstExpr(*expr.lhs, model, table, visiting, diags);
    if (!l)
      return std::nullopt;
    auto r = evalConstExpr(*expr.rhs, model, table, visiting, diags);
    if (!r)
      return std::nullopt;
    if (expr.op == "+")
      return *l + *r;
    if (expr.op == "-")
      return *l - *r;
    if (expr.op == "*")
      return *l * *r;
    if (expr.op == "/") {
      if (*r == 0.0) {
        diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                       "常量表达式除零");
        return std::nullopt;
      }
      return *l / *r;
    }
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                   "初始化值仅支持算术运算（+ - * /）");
    return std::nullopt;
  }
  case ast::ExprKind::Call:
    diags.addError(Location{"", expr.token.line, expr.token.col}, Code::ExprUnsupported,
                   "初始化值不支持函数调用");
    return std::nullopt;
  case ast::ExprKind::If: {
    auto c = evalConstExpr(*expr.cond, model, table, visiting, diags);
    if (!c)
      return std::nullopt;
    const ast::Expr *pick = (*c != 0.0) ? expr.thenExpr.get() : expr.elseExpr.get();
    if (pick == nullptr)
      return std::nullopt;
    return evalConstExpr(*pick, model, table, visiting, diags);
  }
  }
  return std::nullopt;
}

TranslationPlan buildPlanFromEqModule(const ast::Model &model, const SymbolTable &table,
                                      const eqir::EqModule &mod, DiagnosticCollector &diags) {
  TranslationPlan plan;
  plan.modelName = model.nameTok.lexeme;

  std::set<std::string> statesSet(mod.states.begin(), mod.states.end());
  std::set<std::string> visiting;

  // 活跃代数量：出现在 schedule Assign/Solve 中的
  std::set<std::string> activeAlg;
  for (const auto &step : mod.schedule) {
    if (step.kind == eqir::StepKind::Assign)
      activeAlg.insert(step.assign.var);
    else {
      for (const auto &t : step.solve.tearVars)
        activeAlg.insert(t);
    }
  }

  for (const auto &comp : model.components) {
    PlanVar var = makeVar(comp.nameTok.lexeme, comp.type);
    switch (comp.kind) {
    case ast::Component::Kind::Constant:
    case ast::Component::Kind::Parameter: {
      TranslationPlan::ConstantParam cp;
      cp.var = var;
      if (comp.value) {
        auto value = evalConstExpr(*comp.value, model, table, &visiting, diags);
        if (value) {
          if (comp.type == ast::Component::DeclType::Boolean)
            cp.initLiteral = (*value != 0.0) ? "true" : "false";
          else
            cp.initLiteral = literalFromToken(comp.value->token, *value);
        } else {
          cp.initLiteral = "0";
        }
      } else {
        cp.initLiteral = comp.type == ast::Component::DeclType::Boolean ? "false" : "0";
      }
      plan.constantsParams.push_back(std::move(cp));
      break;
    }
    case ast::Component::Kind::Variable:
      if (statesSet.count(comp.nameTok.lexeme)) {
        plan.states.push_back(var);
        double startValue = 0.0;
        if (comp.start) {
          auto value = evalConstExpr(*comp.start, model, table, &visiting, diags);
          if (value)
            startValue = *value;
        }
        plan.stateInitLiterals.push_back(
            literalFromToken(comp.start ? comp.start->token : Token{}, startValue));
        auto it = mod.stateRhs.find(comp.nameTok.lexeme);
        plan.stateRhs.push_back(it != mod.stateRhs.end() ? it->second : nullptr);
      }
      plan.outputOrder.push_back(var);
      break;
    }
  }

  // 代数 Vars：active + aliases
  std::set<std::string> algDeclared;
  for (const auto &step : mod.schedule) {
    if (step.kind == eqir::StepKind::Assign) {
      TranslationPlan::AlgStep s;
      s.kind = TranslationPlan::AlgStep::Kind::Assign;
      const SymbolInfo *info = table.find(step.assign.var);
      s.var = makeVar(step.assign.var, info ? info->type : ast::Component::DeclType::Real);
      s.rhs = step.assign.rhs;
      plan.algebraic.push_back(std::move(s));
      algDeclared.insert(step.assign.var);
    } else {
      TranslationPlan::AlgStep s;
      s.kind = TranslationPlan::AlgStep::Kind::Solve;
      for (const auto &name : step.solve.tearVars) {
        const SymbolInfo *info = table.find(name);
        s.tearVars.push_back(makeVar(name, info ? info->type : ast::Component::DeclType::Real));
        algDeclared.insert(name);
      }
      s.residuals = step.solve.residuals;
      plan.algebraic.push_back(std::move(s));
    }
  }

  for (const auto &[name, info] : mod.aliases) {
    TranslationPlan::AliasBind bind;
    const SymbolInfo *sym = table.find(name);
    bind.var = makeVar(name, sym ? sym->type : ast::Component::DeclType::Real);
    bind.isConst = info.isConst;
    bind.scale = info.scale;
    if (info.isConst) {
      bind.constLiteral = formatReal(info.constValue);
    } else {
      bind.canonicalCpp = mapToCppIdentifier(info.canonical);
    }
    plan.aliasBinds.push_back(std::move(bind));
  }

  plan.startTime = mod.startTime;
  plan.stopTime = mod.stopTime;
  plan.interval = mod.interval;
  const double span = plan.stopTime - plan.startTime;
  long steps = static_cast<long>(std::llround(span / plan.interval));
  if (steps < 1)
    steps = 1;
  plan.steps = steps;
  return plan;
}

TranslationPlan buildPlan(const ast::Model &model, const SymbolTable &table,
                          const EquationAnalysis &analysis, DiagnosticCollector &diags) {
  // 兼容旧路径：仅含赋值步骤的 analysis。
  TranslationPlan plan;
  plan.modelName = model.nameTok.lexeme;
  std::set<std::string> statesSet(analysis.states.begin(), analysis.states.end());
  std::set<std::string> visiting;

  for (const auto &comp : model.components) {
    PlanVar var = makeVar(comp.nameTok.lexeme, comp.type);
    switch (comp.kind) {
    case ast::Component::Kind::Constant:
    case ast::Component::Kind::Parameter: {
      TranslationPlan::ConstantParam cp;
      cp.var = var;
      if (comp.value) {
        auto value = evalConstExpr(*comp.value, model, table, &visiting, diags);
        if (value) {
          if (comp.type == ast::Component::DeclType::Boolean)
            cp.initLiteral = (*value != 0.0) ? "true" : "false";
          else
            cp.initLiteral = literalFromToken(comp.value->token, *value);
        } else {
          cp.initLiteral = "0";
        }
      } else {
        cp.initLiteral = comp.type == ast::Component::DeclType::Boolean ? "false" : "0";
      }
      plan.constantsParams.push_back(std::move(cp));
      break;
    }
    case ast::Component::Kind::Variable:
      if (statesSet.count(comp.nameTok.lexeme)) {
        plan.states.push_back(var);
        double startValue = 0.0;
        if (comp.start) {
          auto value = evalConstExpr(*comp.start, model, table, &visiting, diags);
          if (value)
            startValue = *value;
        }
        plan.stateInitLiterals.push_back(
            literalFromToken(comp.start ? comp.start->token : Token{}, startValue));
        for (const auto &[name, rhs] : analysis.stateDefs) {
          if (name == comp.nameTok.lexeme) {
            plan.stateRhs.push_back(rhs);
            break;
          }
        }
      }
      plan.outputOrder.push_back(var);
      break;
    }
  }

  for (const auto &[name, rhs] : analysis.algebraicSteps) {
    const SymbolInfo *info = table.find(name);
    TranslationPlan::AlgStep step;
    step.kind = TranslationPlan::AlgStep::Kind::Assign;
    step.var = makeVar(name, info ? info->type : ast::Component::DeclType::Real);
    step.rhs = rhs;
    plan.algebraic.push_back(std::move(step));
  }

  plan.startTime = 0.0;
  plan.stopTime = 0.0;
  plan.interval = 0.0;
  if (model.experiment) {
    plan.startTime = model.experiment->startTime;
    plan.stopTime = model.experiment->stopTime;
    plan.interval = model.experiment->interval;
  }
  const double span = plan.stopTime - plan.startTime;
  long steps = static_cast<long>(std::llround(span / plan.interval));
  if (steps < 1)
    steps = 1;
  plan.steps = steps;
  return plan;
}

} // namespace mcdc
