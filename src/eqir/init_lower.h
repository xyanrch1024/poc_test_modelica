#pragma once
#include <optional>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "semantic/symbols.h"

namespace mcdc::eqir {

// 构建初始方程系统：连续方程 + fixed=true 约束 + initial equation；
// 未知量 = 全部 Variable + 各状态的 der(x)。
std::optional<EqModule> lowerInitial(const ast::Model &model, const SymbolTable &table,
                                     const EqModule &continuous, DiagnosticCollector &diags);

} // namespace mcdc::eqir
