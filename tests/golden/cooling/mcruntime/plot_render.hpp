#pragma once
// mcruntime::plot —— 渲染组装（research R5/R7）。
// 失败仅经 RenderStats 表达：不抛出、不改退出码、不写残缺文件（FR-005）。
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "plot_framebuffer.hpp"
#include "plot_mapping.hpp"
#include "png_encoder.hpp"
#include "plot_types.hpp"

namespace mcruntime::plot {

namespace detail {

inline std::string format_tick(double v) {
  char buf[32];
  if (std::fabs(v) >= 1000 || (std::fabs(v) > 0 && std::fabs(v) < 0.01))
    std::snprintf(buf, sizeof(buf), "%.1e", v);
  else
    std::snprintf(buf, sizeof(buf), "%g", v);
  return buf;
}

inline int text_width_px(const std::string& s, int scale) {
  return static_cast<int>(s.size()) * (kFontW + 1) * scale;
}

inline void draw_grid_and_axes(Framebuffer& fb, Range xRange, Range yRange) {
  const int x0 = kMarginLeft, x1 = kWidth - kMarginRight;
  const int yTop = kMarginTop, yBot = kHeight - kMarginBottom;
  const Rgb grid{224, 224, 224}, axis{64, 64, 64};

  const double yStep = nice_step(yRange, 5);
  for (double v = std::ceil(yRange.lo / yStep) * yStep; v <= yRange.hi; v += yStep) {
    const int py = map_value(v, yRange, yBot, yTop);
    fb.draw_line(x0, py, x1, py, grid);
    fb.draw_text(4, py - kFontH / 2, format_tick(v), 1, axis);
  }
  const double xStep = nice_step(xRange, 6);
  for (double v = std::ceil(xRange.lo / xStep) * xStep; v <= xRange.hi; v += xStep) {
    const int px = map_value(v, xRange, x0, x1);
    fb.draw_line(px, yTop, px, yBot, grid);
    fb.draw_text(px - text_width_px(format_tick(v), 1) / 2, yBot + 8,
                 format_tick(v), 1, axis);
  }
  fb.draw_line(x0, yTop, x0, yBot, axis);
  fb.draw_line(x0, yBot, x1, yBot, axis);
}

}  // namespace detail

// 渲染到帧缓冲（不落盘）。返回 false 时 stats 携带失败原因。
// 列校验/坏点统计/量程/曲线/图例/标题全部在此完成。
inline bool render_to_framebuffer(const std::string& title,
                                  const std::vector<TimeSeries>& allSeries,
                                  const PlotRequest& request, Framebuffer& fb,
                                  RenderStats& stats) {
  // ---- 列筛选与校验 ----
  std::vector<const TimeSeries*> chosen;
  if (request.columns.empty()) {
    for (const auto& s : allSeries) chosen.push_back(&s);
  } else {
    for (const auto& want : request.columns) {
      bool found = false;
      for (const auto& s : allSeries) {
        if (s.name == want) {
          chosen.push_back(&s);
          found = true;
          break;
        }
      }
      if (!found) {
        stats.skipReason = "unknown columns";
        std::string avail;
        for (const auto& s : allSeries) {
          avail += avail.empty() ? "" : ", ";
          avail += s.name;
        }
        stats.detail = "未知列 \"" + want + "\"；可用列: " + avail;
        return false;
      }
    }
  }
  if (chosen.empty()) {
    stats.skipReason = "no series";
    stats.detail = "没有可绘制的数据序列";
    return false;
  }

  // ---- 统一量程 ----
  std::vector<double> allTimes, allValues;
  for (const auto* s : chosen) {
    allTimes.insert(allTimes.end(), s->times.begin(), s->times.end());
    allValues.insert(allValues.end(), s->values.begin(), s->values.end());
  }
  const Range xRange = auto_range(allTimes);
  const Range yRange = auto_range(allValues);
  if (!xRange.valid || !yRange.valid) {
    stats.skipReason = "no valid data";
    stats.detail = "数据全部为非有限值";
    return false;
  }

  // ---- 版面：白底/网格/轴/标题/标签 ----
  const Rgb black{32, 32, 32};
  detail::draw_grid_and_axes(fb, xRange, yRange);
  fb.draw_text((kWidth - detail::text_width_px(title, 2)) / 2, 14, title, 2, black);
  fb.draw_text(kWidth - kMarginRight - detail::text_width_px("time", 1),
               kHeight - kMarginBottom + 26, "time", 1, black);
  fb.draw_text(4, 12, "value", 1, black);

  // ---- 曲线与图例 ----
  int legendY = kMarginTop + 6;
  int colorIdx = 0;
  for (const auto* s : chosen) {
    long skipped = 0;
    auto pts = map_series(*s, xRange, yRange, &skipped);
    stats.skippedPoints += skipped;
    const Rgb color = kPalette[colorIdx % 8];
    if (pts.empty()) {
      ++stats.skippedSeries;
      continue;
    }
    if (pts.size() == 1) {
      fb.fill_rect(pts[0].x - 1, pts[0].y - 1, 3, 3, color);  // 单点方块
    } else {
      for (std::size_t i = 1; i < pts.size(); ++i) {
        fb.draw_line(pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y, color);
      }
    }
    ++stats.drawnSeries;

    const int nameW = detail::text_width_px(s->name, 1);
    const int lx = kWidth - kMarginRight - nameW - 20;
    fb.fill_rect(lx, legendY, 10, 7, color);
    fb.draw_text(lx + 14, legendY, s->name, 1, black);
    legendY += kFontH + 4;
    ++colorIdx;
  }
  return stats.drawnSeries > 0;
}

// 组合入口：渲染 + 原子写出 PNG。失败时 stats.ok=false 且不留文件。
inline RenderStats render_and_save(const std::string& title,
                                   const std::vector<TimeSeries>& allSeries,
                                   const PlotRequest& request) {
  RenderStats stats;
  Framebuffer fb;
  if (!render_to_framebuffer(title, allSeries, request, fb, stats)) {
    return stats;
  }
  std::string err;
  const std::string path =
      request.outputPath.empty() ? title + "_result.png" : request.outputPath;
  if (!write_png_atomic(path, kWidth, kHeight, fb.raw(), &err)) {
    stats.skipReason = "write failed";
    stats.detail = err;
    return stats;
  }
  stats.ok = true;
  stats.outputPath = path;
  return stats;
}

}  // namespace mcruntime::plot
