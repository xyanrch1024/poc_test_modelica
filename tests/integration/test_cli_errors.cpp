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

// ---- 条件构造非法输入（spec 003 T028）----

namespace {

// 条件方程：缺 else 且条件非编译期常量 → MC0305。
const char *kCondNoElseSource = R"(model M
  Real v;
  Real x(start = 0, fixed = true);
equation
  if x < 1 then
    v = 1;
  end if;
  der(x) = 0.5;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)";

// 条件表达式：条件为非布尔的数值 → MC0204。
const char *kCondNonBoolSource = R"(model M
  Real y;
  Real x(start = 0, fixed = true);
equation
  y = if x then 1 else 2;
  der(x) = 0.5;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)";

// 条件方程：各分支求解不同未知量 → MC0305。
const char *kCondDiffLhsSource = R"(model M
  Real y;
  Real v;
  Boolean b;
equation
  if b then
    y = 1;
  else
    v = 2;
  end if;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)";

// der(...) 出现在条件表达式内 → MC0203。
const char *kCondDerInConditionSource = R"(model M
  Real y;
  Real x(start = 0, fixed = true);
equation
  y = if der(x) > 0 then 1 else 2;
  der(x) = 0.5;
  annotation(experiment(StopTime = 2, Interval = 0.5));
end M;
)";

} // namespace

TEST(CliErrors, ConditionalMissingElseMC0305) {
  const auto r = runTranslateExpect(kCondNoElseSource, "cli_if_err_1");
  // 缺 else → v 在条件之外永不被定义；语义已按 MC0305 报错。
  EXPECT_EQ(r.exitCode, 2);
  EXPECT_NE(r.stderrText.find("[MC0305]"), std::string::npos);
  EXPECT_FALSE(r.outputDirExists);
}

TEST(CliErrors, ConditionalBadRootTypeMC0204) {
  const auto r = runTranslateExpect(kCondNonBoolSource, "cli_if_err_2");
  EXPECT_EQ(r.exitCode, 2);
  EXPECT_NE(r.stderrText.find("[MC0204]"), std::string::npos);
  // 定位到条件 token（x 在第 5 行）。
  EXPECT_NE(r.stderrText.find("5:"), std::string::npos);
  EXPECT_FALSE(r.outputDirExists);
}

TEST(CliErrors, ConditionalDifferentLhsMC0305) {
  const auto r = runTranslateExpect(kCondDiffLhsSource, "cli_if_err_3");
  EXPECT_EQ(r.exitCode, 2);
  EXPECT_NE(r.stderrText.find("[MC0305]"), std::string::npos);
  EXPECT_FALSE(r.outputDirExists);
}

TEST(CliErrors, ConditionalDerInConditionMC0203) {
  const auto r = runTranslateExpect(kCondDerInConditionSource, "cli_if_err_4");
  EXPECT_EQ(r.exitCode, 2);
  EXPECT_NE(r.stderrText.find("[MC0203]"), std::string::npos);
  EXPECT_FALSE(r.outputDirExists);
}

} // namespace
