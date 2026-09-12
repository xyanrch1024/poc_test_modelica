#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "harness.h"

namespace {

using namespace itest;

// SC-003：≤500 行模型的端到端转换 <10 秒。
TEST(Performance, Translates500LineModelUnderTenSeconds) {
  const std::string workDir = "perf_tmp";
  cleanDir(workDir);
  fs::create_directories(workDir);

  // 程序化合成 ~485 行模型：160 参数 + 160 状态 + 160 方程（≤500 行，SC-003）。
  std::string src = "model Big\n";
  for (int i = 0; i < 160; ++i) {
    src += "  parameter Real p" + std::to_string(i) + " = " + std::to_string(i) + " * 0.001;\n";
  }
  for (int i = 0; i < 160; ++i) {
    src +=
        "  Real x" + std::to_string(i) + "(start = " + std::to_string(i + 1) + ", fixed = true);\n";
  }
  src += "equation\n";
  for (int i = 0; i < 160; ++i) {
    src += "  der(x" + std::to_string(i) + ") = -p" + std::to_string(i) + " * x" +
           std::to_string(i) + ";\n";
  }
  src += "  annotation(experiment(StopTime = 1, Interval = 0.1));\nend Big;\n";

  const std::string modelPath = workDir + "/big.mo";
  {
    std::ofstream out(modelPath, std::ios::binary);
    out << src;
  }
  const int lineCount = static_cast<int>(splitLines(src).size());
  ASSERT_LE(lineCount, 520);
  ASSERT_GE(lineCount, 400);

  const auto start = std::chrono::steady_clock::now();
  const int rc = run(std::string(MODELICAC_BIN) + " translate " + q(modelPath) + " -o " +
                     q(workDir + "/out") + " > /dev/null 2>&1");
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const double seconds = std::chrono::duration<double>(elapsed).count();

  EXPECT_EQ(rc, 0);
  EXPECT_LT(seconds, 10.0) << "转换耗时 " << seconds << "s，超出 SC-003";
}

// SC-003 回退护栏 + Edge Cases"嵌套过深"：500 层嵌套条件表达式转换应快速且不崩溃。
TEST(Performance, DeepNestedConditionalTranslatesFast) {
  const std::string workDir = "perf_nested_tmp";
  cleanDir(workDir);
  fs::create_directories(workDir);

  std::string rhs = "1";
  for (int i = 0; i < 500; ++i)
    rhs = "if s > 0 then (" + rhs + ") else 0";

  std::string src = "model Deep\n  Real y;\n  Real s(start = 1, fixed = true);\nequation\n";
  src += "  y = " + rhs + ";\n  der(s) = 0;\n";
  src += "  annotation(experiment(StopTime = 1, Interval = 0.1));\nend Deep;\n";

  const std::string modelPath = workDir + "/deep.mo";
  {
    std::ofstream out(modelPath, std::ios::binary);
    out << src;
  }

  const auto start = std::chrono::steady_clock::now();
  const int rc = run(std::string(MODELICAC_BIN) + " translate " + q(modelPath) + " -o " +
                     q(workDir + "/out") + " > /dev/null 2>&1");
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const double seconds = std::chrono::duration<double>(elapsed).count();

  EXPECT_EQ(rc, 0);
  EXPECT_LT(seconds, 10.0) << "500 层嵌套转换耗时 " << seconds << "s，护栏失效";
}

} // namespace
