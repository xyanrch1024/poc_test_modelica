#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "harness.h"

namespace {

using namespace itest;

// quickstart.md 场景 A 的自动化版本（用户故事 1、SC-001/SC-002 抽样）。
struct E2eResult {
  int exitCode = -1;
  std::string csv;
  bool csvExists = false;
};

E2eResult translateBuildRun(const std::string &modelFile, const std::string &exeName,
                            const std::string &workDir, const std::string &resultName) {
  cleanDir(workDir);
  fs::create_directories(workDir + "/run");

  const std::string proj = absPath(workDir + "/proj");
  const std::string runDir = absPath(workDir + "/run");
  const int rcTranslate =
      run(std::string(MODELICAC_BIN) + " translate " + q(modelFile) + " -o " + q(proj));
  E2eResult r;
  r.exitCode = rcTranslate;
  if (rcTranslate != 0)
    return r;

  const int rcConfigure = run("cmake -S " + q(proj) + " -B " + q(proj + "/build") +
                              " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1");
  if (rcConfigure != 0) {
    r.exitCode = rcConfigure;
    return r;
  }
  const int rcBuild = run("cmake --build " + q(proj + "/build") + " --parallel > /dev/null 2>&1");
  if (rcBuild != 0) {
    r.exitCode = rcBuild;
    return r;
  }

  // 注意：重定向文件用相对名——shell 已 cd 进 run 目录。
  const int rcRun =
      run("cd " + q(runDir) + " && " + q(proj + "/build/" + exeName) + " > /dev/null 2>stderr.txt");
  r.exitCode = rcRun;
  r.csvExists = readFile(runDir + "/" + resultName, &r.csv);
  return r;
}

TEST(EndToEnd, CoolingModelFullLoop) {
  const std::string workDir = "e2e_cooling_tmp";
  const auto r = translateBuildRun(std::string(EXAMPLES_DIR) + "/cooling.mo", "Cooling", workDir,
                                   "Cooling_result.csv");
  ASSERT_EQ(r.exitCode, 0) << "转换/构建/运行失败";
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 502u); // 表头 + 初始行 + 500 步
  EXPECT_EQ(lines[0], "time,T"); // 声明序列

  // 解析首行/末行
  const auto row = [](const std::string &line) {
    std::vector<double> vals;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, ','))
      vals.push_back(std::stod(cell));
    return vals;
  };
  const auto first = row(lines[1]);
  const auto last = row(lines.back());
  EXPECT_NEAR(first[0], 0.0, 1e-12);
  EXPECT_NEAR(first[1], 90.0, 1e-9);
  EXPECT_NEAR(last[0], 5.0, 1e-9);
  // 解析解：20 + 70·e^{-2.5} ≈ 25.7459；RK4(h=0.01) 应在 1e-6 内吻合
  EXPECT_NEAR(last[1], 25.7459, 1e-3);
}

TEST(EndToEnd, ParameterChangeChangesTrajectory) {
  // spec 用户故事 1 场景 3：参数变化必须改变结果轨迹。
  const std::string workDir = "e2e_param_tmp";
  const auto r = translateBuildRun(std::string(EXAMPLES_DIR) + "/cooling_param.mo", "CoolingParam",
                                   workDir, "CoolingParam_result.csv");
  ASSERT_EQ(r.exitCode, 0);
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 202u); // 表头 + 初始行 + 200 步
  EXPECT_EQ(lines[0], "time,T");

  std::stringstream ss(lines.back());
  std::string cell;
  std::getline(ss, cell, ',');
  double tEnd = std::stod(cell);
  std::getline(ss, cell, ',');
  double tFinal = std::stod(cell);
  EXPECT_NEAR(tEnd, 4.0, 1e-9);
  // 25 + 75·e^{-3.2} ≈ 28.0572
  EXPECT_NEAR(tFinal, 28.0572, 1e-3);
}

TEST(EndToEnd, AlgebraicChainAppearsInOutputColumns) {
  // 代数量进入 CSV 输出列（声明序），且拓扑排序保证可构建可运行。
  const std::string workDir = "e2e_chain_tmp";
  const auto r = translateBuildRun(std::string(EXAMPLES_DIR) + "/case_05_alg_chain.mo", "AlgChain",
                                   workDir, "AlgChain_result.csv");
  ASSERT_EQ(r.exitCode, 0);
  ASSERT_TRUE(r.csvExists);
  const auto lines = splitLines(r.csv);
  EXPECT_EQ(lines[0], "time,s,u,v,z");
}

} // namespace
