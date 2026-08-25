#include <gtest/gtest.h>

#include <string>

#include "plot_font.hpp"

namespace {

using namespace mcruntime::plot;

TEST(PlotFont, CoversAllPrintableAscii) {
  EXPECT_EQ(kFontFirst, 32);
  EXPECT_EQ(kFontLast, 126);
  EXPECT_EQ(kFontCount, 95);
}

TEST(PlotFont, GlyphDimensionsAreFiveBySeven) {
  EXPECT_EQ(kFontW, 5);
  EXPECT_EQ(kFontH, 7);
}

TEST(PlotFont, EveryGlyphHasAtLeastOneSetPixelExceptSpace) {
  for (int i = 0; i < kFontCount; ++i) {
    const char ch = static_cast<char>(kFontFirst + i);
    if (ch == ' ') continue;
    bool any = false;
    for (int col = 0; col < kFontW; ++col) any = any || kGlyphs[i][col] != 0;
    EXPECT_TRUE(any) << "字形为空: '" << ch << "'";
  }
}

TEST(PlotFont, OutOfRangeFallsBackToQuestionMark) {
  EXPECT_EQ(glyph_for('~'), glyph_for('~'));
  // 越界（如中文环境可能出现的字节）回退 '?' 字形。
  EXPECT_EQ(glyph_for(static_cast<char>(-30)), glyph_for('?'));
  EXPECT_EQ(glyph_for(static_cast<char>(127)), glyph_for('?'));
}

TEST(PlotFont, DigitOneGlyphIsRecognizableShape) {
  // '1'：中间列应有多行实心
  const std::uint8_t* g = glyph_for('1');
  int centerBits = 0;
  for (int row = 0; row < kFontH; ++row)
    if ((g[2] >> row) & 1) ++centerBits;
  EXPECT_GE(centerBits, 5);
}

}  // namespace
