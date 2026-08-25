#include <gtest/gtest.h>

#include <sys/wait.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "harness.h"

namespace {

using namespace itest;

// 用户故事 2：列筛选与输出路径控制（FR-002/FR-003）。
class PlotColumns : public ::testing::Test {
 protected:
  void SetUp() override {
    workDir_ = absPath("plot_columns_tmp");
    cleanDir(workDir_);
    proj_ = workDir_ + "/proj";
    ASSERT_EQ(run(std::string(MODELICAC_BIN) + " translate " +
                             q(std::string(EXAMPLES_DIR) + "/case_05_alg_chain.mo") +
                             " -o " + q(proj_)),               0);
    ASSERT_EQ(run("cmake -S " + q(proj_) + " -B " + q(proj_ + "/build") +
                             " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1"),               0);
    ASSERT_EQ(run("cmake --build " + q(proj_ + "/build") +
                             " --parallel > /dev/null 2>&1"),               0);
  }

  // 在独立子目录运行并返回退出码（runName 区分各用例）。
  int runWith(const std::string& runName, const std::string& extraArgs) {
    const std::string runDir = workDir_ + "/" + runName;
    fs::create_directories(runDir);
    return run("cd " + q(runDir) + " && " + q(proj_ + "/build/AlgChain") +
                          " " + extraArgs + " > stdout.txt 2>stderr.txt");
  }

  std::string workDir_;
  std::string proj_;
};

TEST_F(PlotColumns, SingleColumnSelectionSucceeds) {
  EXPECT_EQ(runWith("single", "--plot --plot-columns=s"), 0);
  EXPECT_TRUE(fs::exists(workDir_ + "/single/AlgChain_result.png"));
}

TEST_F(PlotColumns, MultiColumnSameChart) {
  EXPECT_EQ(runWith("multi", "--plot --plot-columns=s,z,u,v"), 0);
  const std::string multiPng = workDir_ + "/multi/AlgChain_result.png";
  EXPECT_TRUE(fs::exists(multiPng));

  // 单列与多列产物必须不同（曲线数量不同）
  EXPECT_EQ(runWith("one", "--plot --plot-columns=s"), 0);
  std::string a, b;
  ASSERT_TRUE(readFile(multiPng, &a));
  ASSERT_TRUE(readFile(workDir_ + "/one/AlgChain_result.png", &b));
  EXPECT_NE(a, b);
}

TEST_F(PlotColumns, CustomOutputPathWithMissingParentDirs) {
  const std::string out = workDir_ + "/custom/deep/dir/sz.png";
  EXPECT_EQ(runWith("custom", "--plot --plot-columns=s,z --plot-output=" + out), 0);
  EXPECT_TRUE(fs::exists(out));
}

}  // namespace
