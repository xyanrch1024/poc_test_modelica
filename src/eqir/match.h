#pragma once
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"

namespace mcdc::eqir {

// 将每条代数方程匹配到 preferredUnknown（赋值形态）；校验完备性。
bool matchEquations(EqModule &mod, DiagnosticCollector &diags);

} // namespace mcdc::eqir
