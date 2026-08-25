#include <gtest/gtest.h>

#include <string>

#include "plot_framebuffer.hpp"

namespace {

using namespace mcruntime::plot;

constexpr Rgb kRed{255, 0, 0};

TEST(Framebuffer, StartsAllWhite) {
  Framebuffer fb;
  EXPECT_EQ(fb.pixel(0, 0), (Rgb{255, 255, 255}));
  EXPECT_EQ(fb.pixel(kWidth - 1, kHeight - 1), (Rgb{255, 255, 255}));
}

TEST(Framebuffer, SetPixelAndProbe) {
  Framebuffer fb;
  fb.set_pixel(10, 20, kRed);
  EXPECT_EQ(fb.pixel(10, 20), kRed);
  EXPECT_EQ(fb.pixel(11, 20), (Rgb{255, 255, 255}));
}

TEST(Framebuffer, OutOfBoundsIsSafeNoOp) {
  Framebuffer fb;
  // 不得崩溃；完全越界的绘制不污染画布。
  fb.set_pixel(-1, -1, kRed);
  fb.set_pixel(kWidth, kHeight, kRed);
  fb.draw_line(-10, -10, -1, -1, kRed);  // 整条线都在画布外
  fb.fill_rect(-3, -3, 3, 3, kRed);
  EXPECT_EQ(fb.pixel(0, 0), (Rgb{255, 255, 255}));
}

TEST(Framebuffer, HorizontalLinePixels) {
  Framebuffer fb;
  fb.draw_line(10, 10, 20, 10, kRed);
  for (int x = 10; x <= 20; ++x) EXPECT_EQ(fb.pixel(x, 10), kRed) << "x=" << x;
}

TEST(Framebuffer, VerticalLinePixels) {
  Framebuffer fb;
  fb.draw_line(15, 5, 15, 25, kRed);
  for (int y = 5; y <= 25; ++y) EXPECT_EQ(fb.pixel(15, y), kRed) << "y=" << y;
}

TEST(Framebuffer, DiagonalLineEndpointsAndContinuity) {
  Framebuffer fb;
  const int n = 30;
  fb.draw_line(0, 0, n, n, kRed);
  // 对角线：每个 y 至少一个红色像素（连续性）
  for (int y = 0; y <= n; ++y) {
    bool found = false;
    for (int x = 0; x <= n; ++x)
      if (fb.pixel(x, y) == kRed) found = true;
    EXPECT_TRUE(found) << "对角线在 y=" << y << " 断裂";
  }
}

TEST(Framebuffer, FillRectCoversExactly) {
  Framebuffer fb;
  fb.fill_rect(4, 4, 3, 2, kRed);
  for (int y = 4; y < 6; ++y)
    for (int x = 4; x < 7; ++x) EXPECT_EQ(fb.pixel(x, y), kRed);
  EXPECT_EQ(fb.pixel(7, 4), (Rgb{255, 255, 255}));
}

TEST(Framebuffer, DrawTextWritesGlyphPixels) {
  Framebuffer fb;
  fb.draw_text(2, 2, "A", 1, {0, 0, 0});
  // 'A' 字形必有实心像素
  bool any = false;
  for (int dy = 0; dy < kFontH && !any; ++dy)
    for (int dx = 0; dx < kFontW && !any; ++dx)
      if (fb.pixel(2 + dx, 2 + dy) == (Rgb{0, 0, 0})) any = true;
  EXPECT_TRUE(any);
}

TEST(Framebuffer, TextWidthFormula) {
  EXPECT_EQ(Framebuffer().text_width("abc", 1), 3 * 6);
  EXPECT_EQ(Framebuffer().text_width("ab", 2), 2 * 6 * 2);
}

}  // namespace
