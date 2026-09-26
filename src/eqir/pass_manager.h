#pragma once
#include <optional>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "semantic/symbols.h"

namespace mcdc::eqir {

// Lower → Match → Alias → Match → BLT → Tear → Schedule
std::optional<EqModule> runBackend(const ast::Model &model, const SymbolTable &table,
                                   DiagnosticCollector &diags);

} // namespace mcdc::eqir
