#include <gtest/gtest.h>

#include <sys/wait.h>

#include <filesystem>
#include <string>

#include "harness.h"

namespace {

using namespace itest;

int exitCodeOf(int status) {
  if (status < 0)
    return status; // run() 已编码信号终止为负值
  return status;
}

// spec 边缘用例：初值导致数值发散的模型 → 生成程序必须明确提示并退出码 3，
// 且删除半成品 CSV（contracts/generated-program-contract.md）。
TEST(Divergence, ReportsDivergenceRemovesCsvExit3) {
  const std::string workDir = "divergence_tmp";
  cleanDir(workDir);
  fs::create_directories(workDir + "/run");

  const std::string proj = absPath(workDir + "/proj");
  const std::string runDir = absPath(workDir + "/run");
  ASSERT_EQ(exitCodeOf(run(std::string(MODELICAC_BIN) + " translate " +
                           q(std::string(EXAMPLES_DIR) + "/diverge.mo") + " -o " + q(proj))),
            0);
  ASSERT_EQ(exitCodeOf(run("cmake -S " + q(proj) + " -B " + q(proj + "/build") +
                           " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1")),
            0);
  ASSERT_EQ(exitCodeOf(run("cmake --build " + q(proj + "/build") + " --parallel > /dev/null 2>&1")),
            0);

  // 不使用 cd：exe 以绝对路径运行，CSV/重定向落在当前工作目录。
  const std::string stderrPath = fs::absolute(workDir + "/run/stderr.txt").string();
  const std::string cmd = q(proj + "/build/Diverge") + " > /dev/null 2>" + q(stderrPath);
  const int status = run(cmd);
  if (status != 3) {
    std::string content;
    readFile(stderrPath, &content);
    ADD_FAILURE() << "cmd=[" << cmd << "] status=" << status << " stderr=[" << content << "]";
    return;
  }
  EXPECT_EQ(exitCodeOf(status), 3);

  std::string stderrText;
  ASSERT_TRUE(readFile(stderrPath, &stderrText));
  EXPECT_NE(stderrText.find("数值发散"), std::string::npos);

  // 半成品 CSV 必须被删除（写在进程当前目录）。
  EXPECT_FALSE(fs::exists(fs::current_path() / "Diverge_result.csv"));
}

} // namespace
