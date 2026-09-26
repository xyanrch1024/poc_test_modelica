#pragma once
#include "diagnostics/diagnostic.h"
#include "eqir/ir.h"

namespace mcdc::eqir {

// 将 BLT 块展开为 Assign / Solve 步骤序列。
bool buildSchedule(EqModule &mod, DiagnosticCollector &diags);

} // namespace mcdc::eqir
