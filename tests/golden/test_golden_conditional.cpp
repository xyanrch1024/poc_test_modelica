#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cli/pipeline.h"
#include "diagnostics/diagnostic.h"

namespace {

using namespace mcdc;

// 金样测试：if 条件表达式/条件方程的生成器输出与 tests/golden/conditional/ 基线逐字节一致
// （FR-007 确定性）。基线再生成方式（仅在有意的生成器变更后执行）：
//   ./build/modelicac translate examples/models/case_21_cond_expr.mo -o /tmp/g
//   cp /tmp/g/CMakeLists.txt tests/golden/conditional/
//   cp /tmp/g/model_Case21CondExpr.cpp tests/golden/conditional/
std::string readFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return "";
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

const char *kCase21Source = R"(model Case21CondExpr
  parameter Real y0 = 1.0;
  parameter Real slope = 2.0;
  Real y;
  Real x(start = 0, fixed = true);
equation
  y = if x > 1 then y0 + slope * (x - 1) else y0 + x;
  der(x) = 1;
  annotation(experiment(StartTime = 0, StopTime = 3, Interval = 0.01));
end Case21CondExpr;
)";

TEST(GoldenConditional, GeneratedProjectMatchesBaselineByteForByte) {
  DiagnosticCollector diags;
  const std::string outDir = "golden_cond_run_tmp";
  TranslateOutcome outcome = runTranslate(kCase21Source, "case_21_cond_expr.mo", &outDir, diags);
  ASSERT_TRUE(outcome.ok) << "流水线意外失败";

  const std::string goldenDir = std::string(GOLDEN_DIR) + "/conditional";
  const std::vector<std::string> names = {
      "CMakeLists.txt",
      "model_Case21CondExpr.cpp",
      "mcruntime/rk4.hpp",
      "mcruntime/csv_writer.hpp",
  };
  for (const auto &name : names) {
    const std::string actual = readFile(outDir + "/" + name);
    const std::string expected = readFile(goldenDir + "/" + name);
    ASSERT_FALSE(actual.empty()) << "缺少生成文件: " << name;
    ASSERT_FALSE(expected.empty()) << "缺少基线文件: tests/golden/conditional/" << name;
    EXPECT_EQ(actual, expected) << "金样不一致: " << name;
  }

  std::filesystem::remove_all(outDir);
}

} // namespace