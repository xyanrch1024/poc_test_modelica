#include "cli/pipeline.h"

#include <memory>
#include <optional>

#include "codegen/codegen.h"
#include "codegen/plan.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/equations.h"
#include "semantic/symbols.h"

namespace mcdc {

TranslateOutcome runTranslate(const std::string &sourceText,
                              const std::string &displayFileName,
                              const std::string *explicitOutputDir,
                              DiagnosticCollector &diags) {
  TranslateOutcome outcome;

  // 1. 词法
  Lexer lexer(sourceText);
  auto tokens = lexer.tokenize(diags);
  diags.fillMissingFile(displayFileName);

  // 2. 解析
  Parser parser(std::move(tokens), diags);
  auto model = parser.parseFile();
  if (!model) {
    return outcome;
  }
  outcome.modelName = model->nameTok.lexeme;

  // 3. 符号与表达式
  auto symbols = SymbolTable::build(*model, diags);
  if (symbols) {
    analyzeExpressions(*model, *symbols, diags);
  }

  // 4. 方程分析
  std::optional<EquationAnalysis> analysis;
  if (symbols) {
    analysis = analyzeEquations(*model, *symbols, diags);
  }

  // 5. 计划
  if (!symbols || !analysis) {
    return outcome;
  }
  TranslationPlan plan = buildPlan(*model, *symbols, *analysis, diags);

  // 统一诊断门禁：任何 error 都阻止产物落盘。
  if (diags.hasErrors()) {
    return outcome;
  }

  // 6. 生成（纯内存）→ 落盘
  std::string genErr;
  GeneratedFiles files = generateProject(plan, &genErr);
  if (files.empty()) {
    diags.addError(Location{displayFileName, 0, 0}, Code::Internal,
                   "生成工程失败: " + genErr);
    return outcome;
  }
  outcome.outputDir =
      explicitOutputDir != nullptr ? *explicitOutputDir : "./" + plan.modelName + "_gen";
  std::string err;
  if (!writeProject(files, outcome.outputDir, &err)) {
    diags.addError(Location{displayFileName, 0, 0}, Code::Internal, "写出生成物失败: " + err);
    return outcome;
  }

  outcome.ok = true;
  return outcome;
}

} // namespace mcdc
