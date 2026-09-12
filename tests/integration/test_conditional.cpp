#include <gtest/gtest.h>

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "harness.h"

namespace {

using namespace itest;

// quickstart.md 场景 A/B/C 的自动化版本（spec 003 用户故事 1、SC-001/SC-002）。
struct CondResult {
  int exitCode = -1;
  std::string csv;
  bool csvExists = false;
};

CondResult translateBuildRun(const std::string &modelFile, const std::string &exeName,
                             const std::string &workDir, const std::string &resultName) {
  cleanDir(workDir);
  fs::create_directories(workDir + "/run");

  const std::string proj = absPath(workDir + "/proj");
  const std::string runDir = absPath(workDir + "/run");
  const int rcTranslate =
      run(std::string(MODELICAC_BIN) + " translate " + q(modelFile) + " -o " + q(proj));
  CondResult r;
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

  const int rcRun =
      run("cd " + q(runDir) + " && " + q(proj + "/build/" + exeName) + " > /dev/null 2>stderr.txt");
  r.exitCode = rcRun;
  r.csvExists = readFile(runDir + "/" + resultName, &r.csv);
  return r;
}

// 解析逗号分隔的 CSV 行。
std::vector<double> parseRow(const std::string &line) {
  std::vector<double> vals;
  std::stringstream ss(line);
  std::string cell;
  while (std::getline(ss, cell, ','))
    vals.push_back(std::stod(cell));
  return vals;
}

TEST(Conditional, ConditionalExpressionChangeSlope) {
  // 场景 A：条件表达式 + 参数化折线；RK4 下 y 在 x 越过 1 时切换斜率且连续。
  const std::string workDir = "cond_c21_tmp";
  const auto r = translateBuildRun(std::string(EXAMPLES_DIR) + "/case_21_cond_expr.mo",
                                   "Case21CondExpr", workDir, "Case21CondExpr_result.csv");
  ASSERT_EQ(r.exitCode, 0) << "转换/构建/运行失败";
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 302u); // 表头 + 初始行 + 300 步
  EXPECT_EQ(lines[0], "time,y,x"); // 声明序列

  const auto first = parseRow(lines[1]);
  EXPECT_NEAR(first[0], 0.0, 1e-12);
  EXPECT_NEAR(first[1], 1.0, 1e-9); // y = y0 + x = 1
  EXPECT_NEAR(first[2], 0.0, 1e-12);

  // t=0.5：x=0.5 < 1，仍在 else 分支（y = y0 + x = 1.5）。
  const auto mid = parseRow(lines[51]); // 索引 = 1 + 0.5/0.01
  EXPECT_NEAR(mid[0], 0.5, 1e-9);
  EXPECT_NEAR(mid[2], 0.5, 1e-6);
  EXPECT_NEAR(mid[1], 1.5, 1e-6);

  // 末行：x≈3，y = y0 + slope*(x-1) = 5。
  const auto last = parseRow(lines.back());
  EXPECT_NEAR(last[0], 3.0, 1e-9);
  EXPECT_NEAR(last[2], 3.0, 1e-6);
  EXPECT_NEAR(last[1], 5.0, 1e-6);
}

TEST(Conditional, ConstantConditionAlwaysSameBranch) {
  // 场景 B：条件只引用参数 → 全程同一分支，逐点自洽。
  const std::string workDir = "cond_c22_tmp";
  const auto r = translateBuildRun(std::string(EXAMPLES_DIR) + "/case_22_cond_const.mo",
                                   "Case22CondConst", workDir, "Case22CondConst_result.csv");
  ASSERT_EQ(r.exitCode, 0);
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 12u); // 表头 + 初始行 + 10 步
  EXPECT_EQ(lines[0], "time,y,z");

  for (size_t i = 1; i < lines.size(); ++i) {
    const auto row = parseRow(lines[i]);
    EXPECT_NEAR(row[1], 7.5, 1e-9); // x*1.5
    EXPECT_NEAR(row[2], 7.5, 1e-9); // 嵌套 if 的 then 分支 = y
  }
}

TEST(Conditional, ConditionalEquationAndStateBranch) {
  // 场景 C：条件方程（二分支）+ 状态量条件导数 + 嵌套 if 表达式。
  const std::string workDir = "cond_c23_tmp";
  const auto r = translateBuildRun(std::string(EXAMPLES_DIR) + "/case_23_cond_eq.mo",
                                   "Case23CondEq", workDir, "Case23CondEq_result.csv");
  ASSERT_EQ(r.exitCode, 0);
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 202u); // 表头 + 初始行 + 200 步
  EXPECT_EQ(lines[0], "time,q,r,s");

  const auto first = parseRow(lines[1]);
  EXPECT_NEAR(first[0], 0.0, 1e-12);
  EXPECT_NEAR(first[1], 0.0, 1e-12); // q = 2*s
  EXPECT_NEAR(first[2], 0.0, 1e-12); // r = q
  EXPECT_NEAR(first[3], 0.0, 1e-12);

  // t=0.5：s=0.5 仍在 then 分支（q=2s=1、r=q=1）。
  const auto mid = parseRow(lines[51]);
  EXPECT_NEAR(mid[0], 0.5, 1e-9);
  EXPECT_NEAR(mid[3], 0.5, 1e-6);
  EXPECT_NEAR(mid[1], 1.0, 1e-6);
  EXPECT_NEAR(mid[2], 1.0, 1e-6);

  // t=2：s 已切 else 分支。切换在 s 越过 1 的 RK4 步内生效（无事件精化），
  // 造成 ≤ ~h/6 的过冲，故按步长量级容差断言。
  const auto last = parseRow(lines.back());
  EXPECT_NEAR(last[0], 2.0, 1e-9);
  EXPECT_NEAR(last[3], 3.0, 2e-3); // 理想 3.0（过冲 <h/6）
  EXPECT_NEAR(last[1], last[3] + 1.0, 1e-9);   // q = s + 1
  EXPECT_NEAR(last[2], -last[1], 1e-9);        // r = -q
}

TEST(Conditional, GeneratedOutputDeterministicAcrossRuns) {
  // FR-007：同一模型重复转换+构建+运行，CSV 逐字节一致。
  const std::string workDir = "cond_deterministic_tmp";
  cleanDir(workDir);
  fs::create_directories(workDir + "/run1");
  fs::create_directories(workDir + "/run2");

  const std::string proj = absPath(workDir + "/proj");
  const std::string modelFile = std::string(EXAMPLES_DIR) + "/case_23_cond_eq.mo";
  ASSERT_EQ(run(std::string(MODELICAC_BIN) + " translate " + q(modelFile) + " -o " + q(proj)), 0);
  ASSERT_EQ(run("cmake -S " + q(proj) + " -B " + q(proj + "/build") +
                " -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1"),
            0);
  ASSERT_EQ(run("cmake --build " + q(proj + "/build") + " --parallel > /dev/null 2>&1"), 0);

  const std::string exe = q(proj + "/build/Case23CondEq");
  const std::string r1 = absPath(workDir + "/run1");
  const std::string r2 = absPath(workDir + "/run2");
  ASSERT_EQ(run("cd " + q(r1) + " && " + exe + " > /dev/null 2>&1"), 0);
  ASSERT_EQ(run("cd " + q(r2) + " && " + exe + " > /dev/null 2>&1"), 0);

  std::string csv1, csv2;
  ASSERT_TRUE(readFile(r1 + "/Case23CondEq_result.csv", &csv1));
  ASSERT_TRUE(readFile(r2 + "/Case23CondEq_result.csv", &csv2));
  EXPECT_EQ(csv1, csv2);
}

TEST(Conditional, ElseifChainSwitchesVThreeSegment) {
  // 场景 D（US2）：三段 else if 链 → 每段斜率切换，段间按序取值。
  const std::string workDir = "cond_c24_tmp";
  const auto r =
      translateBuildRun(std::string(EXAMPLES_DIR) + "/case_24_cond_elseif.mo", "Case24CondElseif",
                        workDir, "Case24CondElseif_result.csv");
  ASSERT_EQ(r.exitCode, 0);
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 42u); // 表头 + 初始行 + 40 步
  EXPECT_EQ(lines[0], "time,v,s");

  // t=1.0：仍首段（v=1），s=0.5。
  const auto seg1 = parseRow(lines[11]); // 1 + 1.0/0.1
  EXPECT_NEAR(seg1[0], 1.0, 1e-9);
  EXPECT_NEAR(seg1[1], 1.0, 1e-6);
  EXPECT_NEAR(seg1[2], 0.5, 1e-6);

  // t=2.5：中段（v=2），s≈1.5。
  const auto seg2 = parseRow(lines[26]); // 1 + 2.5/0.1
  EXPECT_NEAR(seg2[0], 2.5, 1e-9);
  EXPECT_NEAR(seg2[1], 2.0, 1e-6);
  EXPECT_NEAR(seg2[2], 1.5, 2e-2); // 阶段采样过冲容差

  // t=4（末行）：末段（v=3），s≈3.5（理想 3.5 + 两处过冲）。
  const auto last = parseRow(lines.back());
  EXPECT_NEAR(last[0], 4.0, 1e-9);
  EXPECT_NEAR(last[1], 3.0, 1e-6);
  EXPECT_NEAR(last[2], 3.5, 2e-2);
}

TEST(Conditional, NestedIfExpressionAndEquationDigits) {
  // 场景 E（US2）：嵌套三元表达式（w）+ 嵌套/条件导数方程（der(s)）。
  const std::string workDir = "cond_c25_tmp";
  const auto r =
      translateBuildRun(std::string(EXAMPLES_DIR) + "/case_25_cond_nested.mo", "Case25CondNested",
                        workDir, "Case25CondNested_result.csv");
  ASSERT_EQ(r.exitCode, 0);
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 22u); // 表头 + 初始行 + 20 步
  EXPECT_EQ(lines[0], "time,s,w");

  const auto first = parseRow(lines[1]);
  EXPECT_NEAR(first[0], 0.0, 1e-12);
  EXPECT_NEAR(first[1], 2.0, 1e-9); // s0=2
  EXPECT_NEAR(first[2], 3.0, 1e-9); // 外层 else：s≥2

  // t=0.5：1<s<2 → 内层 then 分支 w=2。
  const auto mid = parseRow(lines[6]); // 1 + 0.5/0.1
  EXPECT_NEAR(mid[0], 0.5, 1e-9);
  EXPECT_NEAR(mid[1], 1.5, 1e-6); // s
  EXPECT_NEAR(mid[2], 2.0, 1e-6); // w

  // t=1：s≈1（略小于 1），内层 then 分支 w=1。
  const auto atOne = parseRow(lines[11]); // 1 + 1.0/0.1
  EXPECT_NEAR(atOne[0], 1.0, 1e-9);
  EXPECT_NEAR(atOne[1], 1.0, 2e-3); // s
  EXPECT_NEAR(atOne[2], 1.0, 1e-6); // w

  // 末行：s 已 <0.5，der 切 -2 → 更陡下降。
  const auto last = parseRow(lines.back());
  EXPECT_NEAR(last[0], 2.0, 1e-9);
  EXPECT_NEAR(last[1], -0.5, 2e-2); // s
  EXPECT_NEAR(last[2], 1.0, 1e-6); // w（s<1 → 内层 then=1）
}

TEST(Conditional, ConstantConditionIfEquationFolded) {
  // 场景 F（T033）：常量条件 if 方程 + 缺 else → 静态折叠为活跃分支，运行期恒定。
  const std::string workDir = "cond_c26_tmp";
  const auto r = translateBuildRun(std::string(EXAMPLES_DIR) + "/case_26_cond_constif.mo",
                                   "Case26CondConstIf", workDir, "Case26CondConstIf_result.csv");
  ASSERT_EQ(r.exitCode, 0);
  ASSERT_TRUE(r.csvExists);

  const auto lines = splitLines(r.csv);
  ASSERT_EQ(lines.size(), 10u); // 表头 + 初始行 + 8 步
  EXPECT_EQ(lines[0], "time,v,s");

  for (size_t i = 1; i < lines.size(); ++i) {
    const auto row = parseRow(lines[i]);
    EXPECT_NEAR(row[1], 2.0, 1e-9);      // v 恒为 2（常量折叠）
    EXPECT_NEAR(row[2], row[0], 1e-6);   // s = 0.5*v*t = t
  }
  const auto last = parseRow(lines.back());
  EXPECT_NEAR(last[0], 2.0, 1e-9);
  EXPECT_NEAR(last[2], 2.0, 1e-6);
}

} // namespace