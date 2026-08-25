#include <gtest/gtest.h>

#include <sys/wait.h>

#include <filesystem>
#include <string>

#include "harness.h"

namespace {

using namespace itest;

// 用户故事 3：绘图失败诊断 + 退出码中立 + 发散叠加（FR-005，边缘用例）。
class PlotErrors : public ::testing::Test {
 protected:
  void SetUp() override {
    workDir_ = absPath("plot_errors_tmp");
    cleanDir(workDir_);
    // AlgChain 用于列错误
    chainProj_ = workDir_ + "/chain";
    ASSERT_EQ(run(std::string(MODELICAC_BIN) + " translate " +
                             q(std::string(EXAMPLES_DIR) + "/case_05_alg_chain.mo") +
                             " -o " + q(chainProj_)),               0);
    ASSERT_EQ(run("cmake -S " + q(chainProj_) + " -B " + q(chainProj_ + "/build") +
                             " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1"),               0);
    ASSERT_EQ(run("cmake --build " + q(chainProj_ + "/build") +
                             " --parallel > /dev/null 2>&1"),               0);
    // Diverge 用于发散叠加
    divProj_ = workDir_ + "/div";
    ASSERT_EQ(run(std::string(MODELICAC_BIN) + " translate " +
                             q(std::string(EXAMPLES_DIR) + "/diverge.mo") + " -o " +
                             q(divProj_)),               0);
    ASSERT_EQ(run("cmake -S " + q(divProj_) + " -B " + q(divProj_ + "/build") +
                             " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1"),               0);
    ASSERT_EQ(run("cmake --build " + q(divProj_ + "/build") +
                             " --parallel > /dev/null 2>&1"),               0);
  }

  std::string runIn(const std::string& name, const std::string& exe,
                    const std::string& extraArgs) {
    const std::string runDir = workDir_ + "/" + name;
    fs::create_directories(runDir);
    run("cd " + q(runDir) + " && " + q(exe) + " " + extraArgs +
                   " > stdout.txt 2>stderr.txt");
    return runDir;
  }

  std::string workDir_;
  std::string chainProj_;
  std::string divProj_;
};

TEST_F(PlotErrors, UnknownColumnDiagnosticsListAvailableNamesNoPngExitNeutral) {
  const std::string dir = runIn("badcol", chainProj_ + "/build/AlgChain",
                                "--plot --plot-columns=not_a_var");
  EXPECT_TRUE(fs::exists(dir + "/AlgChain_result.csv")) << "CSV 必须照常产出";

  std::string err, out;
  ASSERT_TRUE(readFile(dir + "/stderr.txt", &err));
  EXPECT_NE(err.find("[plot]"), std::string::npos);
  EXPECT_NE(err.find("not_a_var"), std::string::npos);
  EXPECT_NE(err.find("s"), std::string::npos);  // 可用列清单
  EXPECT_FALSE(fs::exists(dir + "/AlgChain_result.png"));

  ASSERT_TRUE(readFile(dir + "/stdout.txt", &out));
  EXPECT_NE(out.find("plot: skipped ("), std::string::npos);
}

TEST_F(PlotErrors, UnwritableOutputPathReportsAndKeepsCsv) {
  // /dev/null/x.png：父路径非目录 → 创建失败
  const std::string dir = runIn("badpath", chainProj_ + "/build/AlgChain",
                                "--plot --plot-output=/dev/null/sub/x.png");
  EXPECT_TRUE(fs::exists(dir + "/AlgChain_result.csv"));

  std::string err;
  ASSERT_TRUE(readFile(dir + "/stderr.txt", &err));
  EXPECT_NE(err.find("[plot]"), std::string::npos);

  std::string out;
  ASSERT_TRUE(readFile(dir + "/stdout.txt", &out));
  EXPECT_NE(out.find("plot: skipped (write failed)"), std::string::npos);
}

TEST_F(PlotErrors, DivergenceWithPlotAbortsPlotKeepsExitThree) {
  const std::string dir =
      runIn("div", divProj_ + "/build/Diverge", "--plot");
  EXPECT_EQ(fs::exists(dir + "/stdout.txt"), true);

  // 重新执行以捕获退出码（runIn 未返回码）
  const int rc = run("cd " + q(dir) + " && " + q(divProj_ + "/build/Diverge") +
                                " --plot > stdout.txt 2>stderr.txt");
  EXPECT_EQ(rc, 3);  // 主仿真退出语义不受影响

  std::string err;
  ASSERT_TRUE(readFile(dir + "/stderr.txt", &err));
  EXPECT_NE(err.find("数值发散"), std::string::npos);
  EXPECT_NE(err.find("[plot]"), std::string::npos);

  EXPECT_FALSE(fs::exists(dir + "/Diverge_result.png"));
  EXPECT_FALSE(fs::exists(dir + "/Diverge_result.csv"));  // 001 契约：发散删除 CSV

  std::string out;
  ASSERT_TRUE(readFile(dir + "/stdout.txt", &out));
  EXPECT_NE(out.find("plot: skipped"), std::string::npos);
}

}  // namespace
