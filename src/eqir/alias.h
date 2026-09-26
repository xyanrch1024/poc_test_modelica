#pragma once
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"

namespace mcdc::eqir {

// 消除 x=±y 与 x=c 形式的平凡代数方程；替换其余 RHS 中的引用。
bool eliminateAliases(EqModule &mod, DiagnosticCollector &diags);

} // namespace mcdc::eqir
