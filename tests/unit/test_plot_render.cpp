#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "plot_render.hpp"

namespace {

using namespace mcruntime::plot;

TimeSeries make_series(const std::string& name, int n, double yScale) {
  TimeSeries s;
  s.name = name;
  for (int i = 0; i < n; ++i) {
    s.times.push_back(i * 0.1);
    s.values.push_back(i * 0.1 * yScale);
  }
  return s;
}

TEST(RenderAndSave, SuccessWritesPngAndReportsStats) {
  std::vector<TimeSeries> series{make_series("x", 11, 1.0)};
  PlotRequest req;
  req.enabled = true;
  req.outputPath = "render_ok_test.png";
  auto stats = render_and_save("Hello", series, req);
  EXPECT_TRUE(stats.ok) << stats.skipReason << " " << stats.detail;
  EXPECT_EQ(stats.drawnSeries, 1);
  EXPECT_EQ(stats.skippedPoints, 0);
  EXPECT_EQ(stats.outputPath, "render_ok_test.png");
  std::remove(stats.outputPath.c_str());
}

TEST(RenderAndSave, UnknownColumnFailsWithAvailableList) {
  std::vector<TimeSeries> series{make_series("x", 5, 1.0), make_series("y", 5, 2.0)};
  PlotRequest req;
  req.enabled = true;
  req.columns = {"nope"};
  auto stats = render_and_save("M", series, req);
  EXPECT_FALSE(stats.ok);
  EXPECT_EQ(stats.skipReason, "unknown columns");
  EXPECT_NE(stats.detail.find("x"), std::string::npos);
  EXPECT_NE(stats.detail.find("y"), std::string::npos);
}

TEST(RenderAndSave, AllNonFiniteDataFailsCleanly) {
  TimeSeries bad;
  bad.name = "bad";
  for (int i = 0; i < 5; ++i) {
    bad.times.push_back(i * 0.1);
    bad.values.push_back(std::nan(""));
  }
  PlotRequest req;
  req.enabled = true;
  auto stats = render_and_save("M", {bad}, req);
  EXPECT_FALSE(stats.ok);
  EXPECT_EQ(stats.skipReason, "no valid data");
}

TEST(RenderToFramebuffer, CurvePassesThroughFirstMidLastProbes) {
  const int n = 101;
  std::vector<TimeSeries> series{make_series("x", n, 1.0)};
  PlotRequest req;
  req.enabled = true;

  Framebuffer fb;
  RenderStats stats;
  ASSERT_TRUE(render_to_framebuffer("Probe", series, req, fb, stats));
  ASSERT_EQ(stats.drawnSeries, 1);

  // 用与实现一致的映射规则独立计算探针点
  Range xr = auto_range(series[0].times);
  Range yr = auto_range(series[0].values);
  const Rgb color = kPalette[0];
  auto probe = [&](double t, double v) {
    return PixelPoint{map_value(t, xr, kMarginLeft, kWidth - kMarginRight),
                      map_value(v, yr, kHeight - kMarginBottom, kMarginTop)};
  };
  const PixelPoint probes[3] = {
      probe(0.0, 0.0),
      probe((n / 2) * 0.1, (n / 2) * 0.1),
      probe((n - 1) * 0.1, (n - 1) * 0.1),
  };
  for (const auto& p : probes) {
    bool found = false;
    for (int dy = -2; dy <= 2 && !found; ++dy)
      for (int dx = -2; dx <= 2 && !found; ++dx)
        if (fb.pixel(p.x + dx, p.y + dy) == color) found = true;
    EXPECT_TRUE(found) << "曲线未经过探针点 (" << p.x << "," << p.y << ")";
  }
}

bool region_nonwhite(const Framebuffer& f, int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x)
      if (f.pixel(x, y) != Rgb{255, 255, 255}) return true;
  return false;
}

TEST(RenderToFramebuffer, TitleLegendAxisRegionsAreDrawn) {
  std::vector<TimeSeries> series{make_series("legendvar", 21, 1.0)};
  PlotRequest req;
  req.enabled = true;

  Framebuffer fb;
  RenderStats stats;
  ASSERT_TRUE(render_to_framebuffer("MyTitle", series, req, fb, stats));

  // 标题区（顶部）、图例区（绘图区右上）、轴标签区（底部）均应有内容
  EXPECT_TRUE(region_nonwhite(fb, kMarginLeft, 10, kWidth - kMarginRight, 40));
  EXPECT_TRUE(region_nonwhite(fb, kWidth - kMarginRight - 200, kMarginTop,
                              kWidth - kMarginRight - 20, kMarginTop + 40));
  EXPECT_TRUE(region_nonwhite(fb, kMarginLeft, kHeight - kMarginBottom + 4,
                              kWidth - kMarginRight, kHeight - 4));
  // 图例色块为调色板第一色
  EXPECT_EQ(fb.pixel(kWidth - kMarginRight - detail::text_width_px("legendvar", 1) - 20 + 3,
                     kMarginTop + 6 + 3), kPalette[0]);
}

TEST(RenderToFramebuffer, ConstantSeriesRendersHorizontalLine) {
  TimeSeries s = make_series("flat", 50, 0.0);  // 全 0 值 → 退化量程
  PlotRequest req;
  Framebuffer fb;
  RenderStats stats;
  ASSERT_TRUE(render_to_framebuffer("Flat", {s}, req, fb, stats));
  // 绘图区内应存在一条水平非白直线（抽查中部行）
  bool foundRow = false;
  for (int y = kMarginTop; y <= kHeight - kMarginBottom && !foundRow; ++y) {
    int cnt = 0;
    for (int x = kMarginLeft; x <= kWidth - kMarginRight; ++x)
      if (fb.pixel(x, y) == kPalette[0]) ++cnt;
    if (cnt > (kWidth - kMarginLeft - kMarginRight) / 2) foundRow = true;
  }
  EXPECT_TRUE(foundRow);
}

}  // namespace
