#pragma once
// mcruntime::plot —— RGB 帧缓冲与光栅化（research R2）。
// 越界绘制为安全无操作（防御性，不崩溃）。
#include <cstdint>
#include <vector>

#include "plot_font.hpp"
#include "plot_types.hpp"

namespace mcruntime::plot {

class Framebuffer {
 public:
  Framebuffer() : buf_(static_cast<std::size_t>(kWidth) * kHeight * 3, 0xFF) {}

  int width() const { return kWidth; }
  int height() const { return kHeight; }

  void set_pixel(int x, int y, Rgb c) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
    const std::size_t idx = (static_cast<std::size_t>(y) * kWidth + x) * 3;
    buf_[idx] = c.r;
    buf_[idx + 1] = c.g;
    buf_[idx + 2] = c.b;
  }

  Rgb pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return {255, 255, 255};
    const std::size_t idx = (static_cast<std::size_t>(y) * kWidth + x) * 3;
    return {buf_[idx], buf_[idx + 1], buf_[idx + 2]};
  }

  void fill_rect(int x, int y, int w, int h, Rgb c) {
    for (int dy = 0; dy < h; ++dy)
      for (int dx = 0; dx < w; ++dx) set_pixel(x + dx, y + dy, c);
  }

  void draw_line(int x0, int y0, int x1, int y1, Rgb c) {
    int dx = x1 - x0, dy = y1 - y0;
    const int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    int err = (dx > dy ? dx : -dy) / 2;
    while (true) {
      set_pixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      const int e2 = err;
      if (e2 > -dx) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dy) {
        err += dx;
        y0 += sy;
      }
    }
  }

  // 点阵文本：scale 为整数放大倍数。非 ASCII 字符以 '?' 渲染。
  void draw_text(int x, int y, const std::string& text, int scale, Rgb c) {
    if (scale < 1) scale = 1;
    int cursor = x;
    for (char ch : text) {
      const std::uint8_t* g = glyph_for(ch);
      for (int col = 0; col < kFontW; ++col) {
        for (int row = 0; row < kFontH; ++row) {
          if ((g[col] >> row) & 1) {
            fill_rect(cursor + col * scale, y + row * scale, scale, scale, c);
          }
        }
      }
      cursor += (kFontW + 1) * scale;
    }
  }

  int text_width(const std::string& text, int scale) const {
    return static_cast<int>(text.size()) * (kFontW + 1) * scale;
  }

  const std::vector<std::uint8_t>& raw() const { return buf_; }

 private:
  std::vector<std::uint8_t> buf_;
};

}  // namespace mcruntime::plot
