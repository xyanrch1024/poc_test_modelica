#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "lexer/lexer.h"
#include "semantic/symbols.h"

namespace mcdc::eqir {

enum class UnknownRole { State, Algebraic };

struct Unknown {
  std::string name;
  UnknownRole role = UnknownRole::Algebraic;
  ast::Component::DeclType type = ast::Component::DeclType::Real;
  size_t declOrder = 0; // 源声明序，确定性排序用
};

// 一条方程：优先保持赋值形态（preferredUnknown = rhs），残差为 preferred - rhs。
// isStateDeriv 表示 der(preferred) = rhs，不参与代数匹配。
struct Equation {
  size_t id = 0;
  bool isStateDeriv = false;
  std::string preferredUnknown; // Lower 时的 LHS / der 目标
  ast::ExprPtr rhs;             // 所有权
  Token locTok;                 // 诊断定位
  // Match 之后：匹配到的未知量（代数方程）；状态方程保持 preferredUnknown。
  std::optional<std::string> matched;
};

struct StrongComponent {
  std::vector<std::string> unknowns; // 声明序稳定
  std::vector<size_t> eqIds;
  bool trivial = true; // 单节点且无自环 → 可赋值
};

struct AliasInfo {
  std::string canonical;
  double scale = 1.0; // x = scale * canonical（±1）；常量别名用 isConst
  bool isConst = false;
  double constValue = 0.0;
};

enum class StepKind { Assign, Solve };

struct SchedAssign {
  std::string var;
  const ast::Expr *rhs = nullptr;
};

struct SchedSolve {
  std::vector<std::string> tearVars;
  std::vector<const ast::Expr *> residuals; // 各 == 0
};

struct SchedStep {
  StepKind kind = StepKind::Assign;
  SchedAssign assign;
  SchedSolve solve;
};

struct EqModule {
  std::string modelName;
  Token modelTok;
  Token nameTok;

  std::vector<Unknown> unknowns;
  std::vector<Equation> equations;
  std::vector<ast::ExprPtr> extras; // 归约/替换产生的额外表达式所有权

  // BLT 输出（代数块，拓扑序）
  std::vector<StrongComponent> blocks;

  // AliasElim：被消除的代数未知量 → 规范名/常量
  std::map<std::string, AliasInfo> aliases;

  // Schedule 输出
  std::vector<SchedStep> schedule;
  std::vector<std::string> states; // 声明序
  std::map<std::string, const ast::Expr *> stateRhs;

  // 实验设置（Lower 校验后填入）
  double startTime = 0.0;
  double stopTime = 0.0;
  double interval = 0.0;
  bool experimentOk = false;
};

const Unknown *findUnknown(const EqModule &m, const std::string &name);
Unknown *findUnknownMutable(EqModule &m, const std::string &name);
Equation *findEquation(EqModule &m, size_t id);
const Equation *findEquation(const EqModule &m, size_t id);

} // namespace mcdc::eqir
