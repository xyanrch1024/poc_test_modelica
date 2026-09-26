#pragma once
#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"
#include "semantic/symbols.h"
#include "simcode/ir.h"

namespace mcdc::simcode {

SimCode createSimCode(const ast::Model &model, const SymbolTable &table, const eqir::EqModule &cont,
                      const eqir::EqModule &init, DiagnosticCollector &diags);

} // namespace mcdc::simcode
