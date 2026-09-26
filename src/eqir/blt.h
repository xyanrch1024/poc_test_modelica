#pragma once
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"

namespace mcdc::eqir {

// 代数依赖图上的 Tarjan SCC → 块下三角序。非平凡块标记 trivial=false。
// 若 SCC 内方程含状态导数形态则报 MC0307。
bool computeBlt(EqModule &mod, DiagnosticCollector &diags);

} // namespace mcdc::eqir
