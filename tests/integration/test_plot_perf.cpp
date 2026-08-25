#include <gtest/gtest.h>

#include <sys/wait.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "harness.h"

namespace {

using namespace itest;

int exitCodeOf(int status) {
  if (!WIFEXITED(status)) return -1000 - WTERMSIG(status);
  return WEXITSTATUS(status);
}

// SC-004 / FR-007：20 万行数据 --plot 增时 ≤10 秒且 png ≤5MB。
TEST(PlotPerf, Handles200kRowsWithinBudget) {
  const std::string workDir = absPath("plot_perf_tmp");
  cleanDir(workDir);
  fs::create_directories(workDir);

  // StopTime=2000, Interval=0.01 → 200001 行
  const std::string src =
      "model PerfBig\n"
      "  parameter Real kp = 0.001;\n"
      "  Real x(start = 1, fixed = true);\n"
      "equation\n"
      "  der(x) = -kp * x;\n"
      "  annotation(experiment(StartTime = 0, StopTime = 2000, Interval = 0.01));\n"
      "end PerfBig;\n";
  const std::string modelPath = workDir + "/perf_big.mo";
  { std::ofstream out(modelPath); out << src; }

  const std::string proj = workDir + "/proj";
  ASSERT_EQ(exitCodeOf(run(std::string(MODELICAC_BIN) + " translate " + q(modelPath) +
                           " -o " + q(proj))),
            0);
  ASSERT_EQ(exitCodeOf(run("cmake -S " + q(proj) + " -B " + q(proj + "/build") +
                           " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1")),
            0);
  ASSERT_EQ(exitCodeOf(run("cmake --build " + q(proj + "/build") +
                           " --parallel > /dev/null 2>&1")),
            0);

  auto timedRun = [&](const std::string& args) {
    fs::create_directories(workDir + "/" + (args.empty() ? "run_plain" : "run_plot"));
    const auto start = std::chrono::steady_clock::now();
    const int rc = exitCodeOf(run("cd " + q(workDir + "/" +
                                            (args.empty() ? "run_plain" : "run_plot")) +
                                  " && " + q(proj + "/build/PerfBig") + " " + args +
                                  " > /dev/null 2>&1"));
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    EXPECT_EQ(rc, 0);
    return sec;
  };

  const double plainSec = timedRun("");
  const double plotSec = timedRun("--plot");
  EXPECT_LT(plotSec - plainSec, 10.0) << "--plot 增时 " << (plotSec - plainSec) << "s";

  std::string png;
  ASSERT_TRUE(readFile(workDir + "/run_plot/PerfBig_result.png", &png));
  EXPECT_LE(png.size(), 5u * 1024 * 1024) << "PNG 超过 5MB（SC-004）";
}

}  // namespace
