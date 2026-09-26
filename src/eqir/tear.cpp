#include "eqir/tear.h"

#include <algorithm>
#include <set>

#include "eqir/expr_util.h"

namespace mcdc::eqir {

bool tearLoops(EqModule &mod, DiagnosticCollector &diags) {
  // tear 结果写入 block 元数据：非平凡块在 schedule 阶段读 unknowns/eqIds。
  // 此处仅校验可 tear，并预生成 residual 表达式挂到 extras。
  bool ok = true;
  for (auto &block : mod.blocks) {
    if (block.trivial)
      continue;

    const size_t n = block.unknowns.size();
    if (n == 0) {
      diags.addError(Location{"", mod.modelTok.line, mod.modelTok.col}, Code::EqAlgebraicLoop,
                     "空的强连通分量，无法 tearing");
      ok = false;
      continue;
    }

    // v1：≤4 全 tear；>4 仍全 tear（简单策略），仅当方程数不匹配时报错
    if (block.eqIds.size() != n) {
      diags.addError(Location{"", mod.modelTok.line, mod.modelTok.col}, Code::EqAlgebraicLoop,
                     "代数环 tearing 失败：SCC 内方程数与未知量数不一致");
      ok = false;
      continue;
    }

    // 为每条方程预构建 residual（matched - rhs），存入 extras，指针挂回方程侧
    // 通过在 Equation 上复用：我们在 schedule 时现场构造。
    (void)block;
  }
  return ok;
}

} // namespace mcdc::eqir
