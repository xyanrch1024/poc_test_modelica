#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "cli/pipeline.h"
#include "diagnostics/diagnostic.h"

namespace {

constexpr const char *kVersion = "modelicac 0.1.0";

int usage() {
  std::cerr << "用法: modelicac translate <input.mo> [-o <outdir>]\n";
  std::cerr << "      modelicac --version\n";
  return 1;
}

bool readWholeFile(const std::string &path, std::string *out) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

void printDiagnostics(const mcdc::DiagnosticCollector &diags) {
  for (const auto &d : diags.sorted()) {
    std::cerr << mcdc::render(d) << "\n";
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc >= 2) {
    const std::string flag = argv[1];
    if (flag == "--version") {
      std::cout << kVersion << "\n";
      return 0;
    }
    if (flag == "--help" || flag == "-h") {
      usage();
      return 1;
    }
  }
  if (argc < 3 || std::string(argv[1]) != "translate") {
    return usage();
  }

  const std::string inputPath = argv[2];
  std::string outputDir;
  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) {
      outputDir = argv[++i];
    } else {
      return usage();
    }
  }

  std::string source;
  if (!readWholeFile(inputPath, &source)) {
    std::cerr << "[error] 无法读取输入文件: " << inputPath << "\n";
    return 1;
  }

  mcdc::DiagnosticCollector diags;
  mcdc::TranslateOutcome outcome =
      mcdc::runTranslate(source, inputPath, outputDir.empty() ? nullptr : &outputDir, diags);

  if (diags.hasErrors()) {
    printDiagnostics(diags);
    return 2;
  }
  if (!outcome.ok) {
    printDiagnostics(diags);
    return 1; // 无诊断却失败 → 视为内部错误路径
  }

  std::cout << "已生成: " << outcome.outputDir << " (模型 " << outcome.modelName << ")\n";
  return 0;
}
