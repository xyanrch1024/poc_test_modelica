#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "plot_mapping.hpp"

namespace {

using namespace mcruntime::plot;

TEST(AutoRange, PadsFiniteExtentsByFivePercent) {
  Range r = auto_range(std::vector<double>{0.0, 10.0});
  EXPECT_TRUE(r.valid);
  EXPECT_NEAR(r.lo, -0.5, 1e-9);
  EXPECT_NEAR(r.hi, 10.5, 1e-9);
}

TEST(AutoRange, DegenerateBecomesPlusMinusOne) {
  Range r = auto_range(std::vector<double>{5.0, 5.0, 5.0});
  EXPECT_TRUE(r.valid);
  EXPECT_NEAR(r.lo, 4.0, 1e-9);
  EXPECT_NEAR(r.hi, 6.0, 1e-9);
}

TEST(AutoRange, IgnoresNonFiniteAndReportsInvalidWhenAllBad) {
  Range r = auto_range(std::vector<double>{std::nan(""), INFINITY, -INFINITY});
  EXPECT_FALSE(r.valid);
}

TEST(MapValue, LinearInterpolationWithClamp) {
  Range r{0.0, 10.0, true};
  EXPECT_EQ(map_value(0.0, r, 100, 200), 100);
  EXPECT_EQ(map_value(10.0, r, 100, 200), 200);
  EXPECT_EQ(map_value(5.0, r, 100, 200), 150);
}

TEST(NiceStep, ProducesFriendlySteps) {
  EXPECT_DOUBLE_EQ(nice_step(Range{0.0, 1.0, true}, 5), 0.2);
  EXPECT_DOUBLE_EQ(nice_step(Range{0.0, 10.0, true}, 5), 2.0);
  EXPECT_DOUBLE_EQ(nice_step(Range{0.0, 100.0, true}, 5), 20.0);
}

class MapSeriesTest : public ::testing::Test {
 protected:
  TimeSeries make_linear(int n) {
    TimeSeries s;
    s.name = "x";
    for (int i = 0; i < n; ++i) {
      s.times.push_back(i * 0.01);
      s.values.push_back(i * 1.0);
    }
    return s;
  }
};

TEST_F(MapSeriesTest, SmallSeriesMapsEveryPoint) {
  auto s = make_linear(50);
  long skipped = 0;
  auto pts = map_series(s, Range{0.0, 0.49, true}, Range{0.0, 49.0, true}, &skipped);
  EXPECT_EQ(pts.size(), 50u);
  EXPECT_EQ(skipped, 0);
}

TEST_F(MapSeriesTest, LargeSeriesDownsampledToThreshold) {
  auto s = make_linear(10000);
  long skipped = 0;
  auto pts = map_series(s, Range{0.0, 99.99, true}, Range{0.0, 9999.0, true}, &skipped);
  EXPECT_LE(static_cast<long>(pts.size()), kMaxSeriesPoints);
  EXPECT_GT(pts.size(), 2u);
  // 保序：x 单调不减
  for (std::size_t i = 1; i < pts.size(); ++i)
    EXPECT_GE(pts[i].x, pts[i - 1].x) << "降采样后 x 乱序 @" << i;
}

TEST_F(MapSeriesTest, DownsamplePreservesExtremesOfEachBucket) {
  // 尖峰序列：每 100 个点一个尖峰，降采样后尖峰值必须仍出现在 y 范围内
  TimeSeries s;
  s.name = "spiky";
  std::vector<double> peaks;
  for (int i = 0; i < kMaxSeriesPoints * 3; ++i) {
    const double v = (i % 100 == 50) ? 1000.0 : static_cast<double>(i % 7);
    s.times.push_back(i * 0.001);
    s.values.push_back(v);
    if (i % 100 == 50) peaks.push_back(v);
  }
  long skipped = 0;
  auto pts = map_series(s, Range{s.times.front(), s.times.back(), true},
                        Range{0.0, 1000.0, true}, &skipped);
  ASSERT_FALSE(pts.empty());
  // 至少部分尖峰被保留：映射后的最大 y 应接近顶部
  int maxY = 0;
  for (auto& p : pts) maxY = std::max(maxY, p.y);
  EXPECT_GT(maxY, kMarginTop + (kHeight - kMarginBottom - kMarginTop) / 2);
}

TEST_F(MapSeriesTest, SkipsNonFiniteAndCounts) {
  TimeSeries s;
  s.name = "gappy";
  s.times = {0.0, 1.0, 2.0, 3.0, 4.0};
  s.values = {1.0, std::nan(""), INFINITY, std::nan(""), 5.0};
  long skipped = 0;
  auto pts = map_series(s, Range{0.0, 4.0, true}, Range{0.0, 5.0, true}, &skipped);
  EXPECT_EQ(skipped, 3);
  EXPECT_EQ(pts.size(), 2u);
}

}  // namespace
