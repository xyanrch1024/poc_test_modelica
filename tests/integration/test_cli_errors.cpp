#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "harness.h"

namespace {

using namespace itest;

struct CliRun {
  int exitCode = -1;
  std::string stderrText;
  bool outputDirExists = false;
};

// 运行 CLI 并捕获 stderr；检查输出目录是否残留。
CliRun runTranslateExpect(const std::string &source, const std::string &outDir) {
  const std::string workDir = "cli_err_tmp";
  cleanDir(workDir);
  fs::create_directories(workDir);
  const std::string modelPath = workDir + "/input.mo";
  {
    std::ofstream out(modelPath, std::ios::binary);
    out << source;
  }
  CliRun r;
  r.exitCode = run(std::string(MODELICAC_BIN) + " translate " + q(modelPath) + " -o " + q(outDir) +
                   " 2>" + q(workDir + "/err.txt"));
  readFile(workDir + "/err.txt", &r.stderrText);
  r.outputDirExists = fs::exists(outDir);
  return r;
}

TEST(CliErrors, UndeclaredIdentifierExit2WithLocationAndNoArtifacts) {
  const auto r =
      runTranslateExpect("model Broken\n  Real x;\nequation\n  x = y + 1;\n"
                         "  annotation(experiment(StopTime = 1, Interval = 0.5));\nend Broken;\n",
                         "cli_err_out_1");
  EXPECT_EQ(r.exitCode, 2);
  EXPECT_NE(r.stderrText.find("[MC0102]"), std::string::npos);
  EXPECT_NE(r.stderrText.find("4:"), std::string::npos); // 行号定位（y 在第 4 行）
  EXPECT_FALSE(r.outputDirExists);                       // 失败零产物
}

TEST(CliErrors, EmptyFileReportsMC0001AndNoArtifacts) {
  const auto r = runTranslateExpect("// 仅注释\n", "cli_err_out_2");
  EXPECT_EQ(r.exitCode, 2);
  EXPECT_NE(r.stderrText.find("[MC0001]"), std::string::npos);
  EXPECT_FALSE(r.outputDirExists);
}

TEST(CliErrors, MultipleModelsReportsMC0002AndNoArtifacts) {
  const auto r = runTranslateExpect(
      "model A\n  Real x(start = 1, fixed = true);\nequation\n  der(x) = 0;\nend A;\n"
      "model B\n  Real y(start = 1, fixed = true);\nequation\n  der(y) = 0;\nend B;\n",
      "cli_err_out_3");
  EXPECT_EQ(r.exitCode, 2);
  EXPECT_NE(r.stderrText.find("[MC0002]"), std::string::npos);
  EXPECT_FALSE(r.outputDirExists);
}

TEST(CliErrors, MissingInputFileExit1) {
  const int rc = run(std::string(MODELICAC_BIN) + " translate 'no_such_file_xyz.mo' 2>/dev/null");
  EXPECT_EQ(rc, 1);
}

TEST(CliErrors, BadUsageExit1) {
  EXPECT_EQ(run(std::string(MODELICAC_BIN) + " 2>/dev/null"), 1);
  EXPECT_EQ(run(std::string(MODELICAC_BIN) + " frobnicate x.mo 2>/dev/null"), 1);
}

} // namespace
