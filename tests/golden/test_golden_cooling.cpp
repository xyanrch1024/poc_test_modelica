#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cli/pipeline.h"
#include "diagnostics/diagnostic.h"

namespace {

using namespace mcdc;

// 金样测试：代码生成输出与 tests/golden/cooling/ 基线逐字节一致（FR-007 确定性）。
// 基线再生成方式（仅在有意的生成器变更后执行）：
//   ./build/modelicac translate examples/models/cooling.mo -o /tmp/g
//   cp /tmp/g/CMakeLists.txt tests/golden/cooling/
//   cp /tmp/g/model_Cooling.cpp tests/golden/cooling/
//   cp /tmp/g/mcruntime/*.hpp tests/golden/cooling/
std::string readFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return "";
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

const char *kCoolingSource = R"(model Cooling
  parameter Real k = 0.5;
  parameter Real Tamb = 20;
  Real T(start = 90, fixed = true);
equation
  der(T) = -k * (T - Tamb);
  annotation(experiment(StartTime = 0, StopTime = 5, Interval = 0.01));
end Cooling;
)";

TEST(GoldenCooling, GeneratedProjectMatchesBaselineByteForByte) {
  DiagnosticCollector diags;
  const std::string outDir = "golden_run_tmp";
  TranslateOutcome outcome = runTranslate(kCoolingSource, "cooling.mo", &outDir, diags);
  ASSERT_TRUE(outcome.ok) << "流水线意外失败";

  const std::string goldenDir = std::string(GOLDEN_DIR) + "/cooling";
  const std::vector<std::string> names = {
      "CMakeLists.txt",
      "model_Cooling.cpp",
      "mcruntime/rk4.hpp",
      "mcruntime/csv_writer.hpp",
  };
  for (const auto &name : names) {
    const std::string actual = readFile(outDir + "/" + name);
    const std::string expected = readFile(goldenDir + "/" + name);
    ASSERT_FALSE(actual.empty()) << "缺少生成文件: " << name;
    ASSERT_FALSE(expected.empty()) << "缺少基线文件: tests/golden/cooling/" << name;
    EXPECT_EQ(actual, expected) << "金样不一致: " << name;
  }

  // 二次运行逐字节一致（同进程内确定性）。
  DiagnosticCollector diags2;
  const std::string outDir2 = "golden_run_tmp2";
  auto second = runTranslate(kCoolingSource, "cooling.mo", &outDir2, diags2);
  ASSERT_TRUE(second.ok);
  for (const auto &name : names) {
    EXPECT_EQ(readFile(outDir + "/" + name), readFile(outDir2 + "/" + name)) << name;
  }

  std::filesystem::remove_all(outDir);
  std::filesystem::remove_all(outDir2);
}

} // namespace
