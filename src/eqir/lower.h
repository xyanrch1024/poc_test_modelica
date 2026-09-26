#pragma once
#include <optional>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "semantic/symbols.h"

namespace mcdc::eqir {

// AST → EqModule：条件方程归约、状态/代数分类、配平与实验校验（不含环拒绝）。
std::optional<EqModule> lower(const ast::Model &model, const SymbolTable &table,
                              DiagnosticCollector &diags);

} // namespace mcdc::eqir
