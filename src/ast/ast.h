#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lexer/lexer.h"

namespace mcdc::ast {

// 表达式节点（data-model.md）。所有节点携带其首 token 的位置用于诊断。
struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class ExprKind { NumLit, BoolLit, Ident, Unary, Binary, Call, Der };

struct Expr {
  ExprKind kind;
  Token token;               // 位置来源
  double numValue = 0.0;     // NumLit
  bool boolValue = false;    // BoolLit
  std::string op;            // Unary/Binary 的运算符；Call 的函数名
  ExprPtr lhs;               // Unary/Binary/Call 无此成员时为空
  ExprPtr rhs;               // Binary
  std::vector<ExprPtr> args; // Call

  static ExprPtr num(Token tok, double value);
  static ExprPtr boolean(Token tok, bool value);
  static ExprPtr ident(Token tok);
  static ExprPtr unary(Token opTok, std::string op, ExprPtr operand);
  static ExprPtr binary(Token opTok, std::string op, ExprPtr l, ExprPtr r);
  static ExprPtr call(Token nameTok, std::vector<ExprPtr> arguments);
  static ExprPtr der(Token derTok, Token targetTok);
};

// 方程一侧：要么是普通表达式，要么是顶层 der(Ident)。
struct EquationSide {
  bool isDer = false;
  Token derToken;  // isDer 时有效（der 关键字位置）
  Token targetTok; // isDer 时有效（被求导变量位置）
  ExprPtr expr;    // 非 der 时有效
};

struct Equation {
  EquationSide lhs;
  EquationSide rhs;
};

struct Component {
  enum class Kind { Constant, Parameter, Variable };
  enum class DeclType { Real, Integer, Boolean };

  Token kindTok; // constant/parameter 或类型关键字位置
  Token typeTok; // Real/Integer/Boolean 位置
  Kind kind = Kind::Variable;
  DeclType type = DeclType::Real;
  Token nameTok;
  ExprPtr value; // "= expr" 初始化
  ExprPtr start; // start 属性
  ExprPtr fixed; // fixed 属性（true/false）
  bool hasStart = false;
  bool hasFixed = false;
};

struct Experiment {
  Token annTok; // annotation 关键字位置
  double startTime = 0.0;
  double stopTime = 0.0;
  double interval = 0.0;
  bool hasStopTime = false;
  bool hasInterval = false;
};

struct Model {
  Token modelTok;
  Token nameTok;
  std::vector<Component> components;
  std::vector<Equation> equations;
  std::optional<Experiment> experiment;
};

} // namespace mcdc::ast
