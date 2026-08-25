#pragma once
#include <string>

#include "diagnostics/diagnostic.h"

namespace mcdc {

struct TranslateOutcome {
  bool ok = false;
  std::string modelName;
  std::string outputDir;
};

// 编译全流程：词法 → 解析 → 符号/表达式 → 方程分析 → 计划 → 生成并落盘。
// - 所有诊断写入 diags（词法阶段自动补全 displayFileName）；
// - 失败时保证不在磁盘创建任何输出（FR-005/FR-006、用户故事 3 场景 1）；
// - explicitOutputDir 为空时使用默认 "./<模型名>_gen"。
TranslateOutcome runTranslate(const std::string &sourceText, const std::string &displayFileName,
                              const std::string *explicitOutputDir, DiagnosticCollector &diags);

} // namespace mcdc
