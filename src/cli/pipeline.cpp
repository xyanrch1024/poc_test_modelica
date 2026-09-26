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

  Lexer lexer(sourceText);
  auto tokens = lexer.tokenize(diags);
  diags.fillMissingFile(displayFileName);

  Parser parser(std::move(tokens), diags);
  auto model = parser.parseFile();
  if (!model) {
    return outcome;
  }
  outcome.modelName = model->nameTok.lexeme;

  auto symbols = SymbolTable::build(*model, diags);
  if (symbols) {
    analyzeExpressions(*model, *symbols, diags);
  }

  std::optional<EquationAnalysis> analysis;
  if (symbols) {
    analysis = analyzeEquations(*model, *symbols, diags);
  }

  if (!symbols || !analysis || !analysis->eqModule) {
    return outcome;
  }
  TranslationPlan plan =
      buildPlanFromEqModule(*model, *symbols, *analysis->eqModule, diags);

  diags.fillMissingFile(displayFileName);

  if (diags.hasErrors()) {
    return outcome;
  }

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
