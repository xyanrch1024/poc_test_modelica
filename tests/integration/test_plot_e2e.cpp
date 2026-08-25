#include <gtest/gtest.h>

#include <sys/wait.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "harness.h"

namespace {

using namespace itest;

// 用户故事 1：--plot 全链路 + 零参数默认行为回归（FR-001）。
class PlotE2E : public ::testing::Test {
 protected:
  void SetUp() override {
    workDir_ = absPath("plot_e2e_tmp");
    cleanDir(workDir_);
    fs::create_directories(workDir_ + "/run_default");
    fs::create_directories(workDir_ + "/run_plot");

    proj_ = workDir_ + "/proj";
    ASSERT_EQ(run(std::string(MODELICAC_BIN) + " translate " +
                             q(std::string(EXAMPLES_DIR) + "/hello.mo") + " -o " +
                             q(proj_)), 0);
    ASSERT_EQ(run("cmake -S " + q(proj_) + " -B " + q(proj_ + "/build") +
                             " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1"),
              0);
    ASSERT_EQ(run("cmake --build " + q(proj_ + "/build") +
                             " --parallel > /dev/null 2>&1"), 0);
  }

  void TearDown() override { /* 保留产物便于失败排查 */ }

  std::string workDir_;
  std::string proj_;
};

TEST_F(PlotE2E, PlotFlagProducesCsvAndPngWithSummaryLine) {
  const std::string runDir = workDir_ + "/run_plot";
  const int rc = run("cd " + q(runDir) + " && " + q(proj_ + "/build/Hello") +
                                " --plot > stdout.txt 2>stderr.txt");
  EXPECT_EQ(rc, 0);
  EXPECT_TRUE(fs::exists(runDir + "/Hello_result.csv"));
  ASSERT_TRUE(fs::exists(runDir + "/Hello_result.png"));

  std::string out;
  ASSERT_TRUE(readFile(runDir + "/stdout.txt", &out));
  EXPECT_NE(out.find("plot: "), std::string::npos);
  EXPECT_NE(out.find("Hello_result.png"), std::string::npos);

  // PNG 头签名
  std::string png;
  ASSERT_TRUE(readFile(runDir + "/Hello_result.png", &png));
  ASSERT_GE(png.size(), 8u);
  EXPECT_EQ(png.substr(0, 4), std::string("\x89PNG", 4));
}

TEST_F(PlotE2E, DefaultRunUnchangedNoPlotArtifacts) {
  const std::string runDir = workDir_ + "/run_default";
  const int rc = run("cd " + q(runDir) + " && " + q(proj_ + "/build/Hello") +
                                " > stdout.txt 2>stderr.txt");
  EXPECT_EQ(rc, 0);
  EXPECT_TRUE(fs::exists(runDir + "/Hello_result.csv"));
  EXPECT_FALSE(fs::exists(runDir + "/Hello_result.png"));

  std::string out;
  ASSERT_TRUE(readFile(runDir + "/stdout.txt", &out));
  EXPECT_EQ(out.find("plot:"), std::string::npos) << "零参数运行不得出现 plot 摘要行";
}

}  // namespace
