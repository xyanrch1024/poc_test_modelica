#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "runtime/csv_writer.hpp"
#include "runtime/rk4.hpp"

namespace {

using namespace mcruntime;

TEST(Rk4, IntegratesExponentialDecayToAnalyticTolerance) {
  // dx/dt = -x, x(0)=2 → x(1) = 2e^{-1}
  const double h = 0.01;
  double t = 0.0;
  std::vector<double> y{2.0};

  auto rhs = [](double, const double *yy, double *dy) { dy[0] = -yy[0]; };

  for (int i = 0; i < 100; ++i) {
    rk4_step(h, t, y, rhs);
  }
  EXPECT_NEAR(t, 1.0, 1e-12);
  const double expected = 2.0 * std::exp(-1.0);
  EXPECT_NEAR(y[0], expected, 1e-9);
}

TEST(Rk4, IsBitwiseDeterministicAcrossRuns) {
  auto run = []() {
    double t = 0.0;
    std::vector<double> y{3.0, -1.5};
    auto rhs = [](double, const double *yy, double *dy) {
      dy[0] = yy[1];
      dy[1] = -yy[0]; // 谐波振荡
    };
    for (int i = 0; i < 250; ++i)
      rk4_step(0.01, t, y, rhs);
    return y;
  };
  const auto a = run();
  const auto b = run();
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    EXPECT_EQ(a[i], b[i]) << "RK4 结果必须逐位一致（FR-007）";
  }
}

TEST(Rk4, ThrowsOnDivergence) {
  double t = 0.0;
  std::vector<double> y{3.0};
  auto rhs = [](double, const double *yy, double *dy) { dy[0] = yy[0] * yy[0]; };
  bool threw = false;
  try {
    for (int i = 0; i < 100 && !threw; ++i)
      rk4_step(0.01, t, y, rhs);
  } catch (const std::runtime_error &e) {
    threw = true;
    EXPECT_NE(std::string(e.what()).find("数值发散"), std::string::npos);
  }
  EXPECT_TRUE(threw);
}

TEST(CsvWriter, WritesSchemaConformantFile) {
  const std::string path = "csv_writer_test_result.csv";
  {
    CsvWriter csv(path, {"time", "T", "z"});
    csv.writeRow(0.0, {90.0, 0.5});
    csv.writeRow(0.01, {89.55, 0.25});
  }
  std::ifstream in(path);
  ASSERT_TRUE(in.good());
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string content = ss.str();
  EXPECT_EQ(content.substr(0, 9), "time,T,z\n");
  EXPECT_NE(content.find(",90,"), std::string::npos); // %.17g 输出
  EXPECT_EQ(std::remove(path.c_str()), 0);
}

} // namespace
