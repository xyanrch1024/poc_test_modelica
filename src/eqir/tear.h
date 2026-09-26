#pragma once
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"

namespace mcdc::eqir {

// 为非平凡 SCC 选择 tear 集。v1：未知量数 ≤4 时全 tear；更大则确定性启发式。
// 无法 tearing 时报 MC0303。
bool tearLoops(EqModule &mod, DiagnosticCollector &diags);

} // namespace mcdc::eqir
