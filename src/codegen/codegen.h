#pragma once
#include <map>
#include <string>

#include "codegen/plan.h"

namespace mcdc {

// 生成工程（data-model.md GeneratedProject）：文件名 → 全部文本内容。
using GeneratedFiles = std::map<std::string, std::string>;

// 纯函数：由翻译计划生成全部文件内容。运行时源文件从 MC_RUNTIME_SRC_DIR
// 读入以避免双份维护；失败返回空 map 并填充 err。
GeneratedFiles generateProject(const TranslationPlan &plan, std::string *err);

// 将生成内容写入 outDir（先全部成功构建字符串，再统一落盘）。
// 失败时返回 false 并填充 err。
bool writeProject(const GeneratedFiles &files, const std::string &outDir, std::string *err);

} // namespace mcdc
