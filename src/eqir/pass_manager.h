#pragma once
#include <optional>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "semantic/symbols.h"

namespace mcdc::eqir {

// Lower → Match → Alias → Match → BLT → Tear → Schedule（连续系统）
std::optional<EqModule> runBackend(const ast::Model &model, const SymbolTable &table,
                                   DiagnosticCollector &diags);

// 初始方程系统（依赖已成功的连续 EqModule）
std::optional<EqModule> runInitialBackend(const ast::Model &model, const SymbolTable &table,
                                          const EqModule &continuous,
                                          DiagnosticCollector &diags);

} // namespace mcdc::eqir
