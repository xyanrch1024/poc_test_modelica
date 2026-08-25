#pragma once
// mcruntime::plot —— 数据→像素映射、自动量程、等宽桶降采样（research R4）。
#include <algorithm>
#include <cmath>
#include <vector>

#include "plot_types.hpp"

namespace mcruntime::plot {

struct PixelPoint {
  int x, y;
};

struct Range {
  double lo = 0.0, hi = 1.0;
  bool valid = false;
};

inline bool is_finite(double v) { return std::isfinite(v); }

// 自动量程：仅统计有限值；两端外扩 5%；min==max → ±1（水平线居中）。
template <typename ValuesT>
Range auto_range(const ValuesT& values) {
  double lo = 0, hi = 0;
  bool any = false;
  for (double v : values) {
    if (!is_finite(v)) continue;
    if (!any) {
      lo = hi = v;
      any = true;
    } else {
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  }
  if (!any) return {0.0, 1.0, false};
  if (lo == hi) return {lo - 1.0, hi + 1.0, true};
  const double pad = (hi - lo) * 0.05;
  return {lo - pad, hi + pad, true};
}

// 线性映射到 [outLo, outHi] 整数像素，越界裁剪。
inline int map_value(double v, Range r, int outLo, int outHi) {
  if (!r.valid || r.hi == r.lo) return outLo;
  const double frac = (v - r.lo) / (r.hi - r.lo);
  const double p = static_cast<double>(outLo) +
                   frac * static_cast<double>(outHi - outLo);
  return static_cast<int>(std::lround(p));
}

// nice 刻度：约 count 条主刻度的"好看"步长。
inline double nice_step(Range r, int targetCount) {
  const double span = r.hi - r.lo;
  if (!(span > 0)) return 1.0;
  const double rough = span / std::max(1, targetCount);
  const double mag = std::pow(10.0, std::floor(std::log10(rough)));
  double step = mag;
  for (double m : {1.0, 2.0, 5.0, 10.0}) {
    if (rough <= m * mag) {
      step = m * mag;
      break;
    }
  }
  return step;
}

// 单序列映射：剔除非有限点并计数；超过阈值时做等宽桶 min/max 保形压缩。
// 输出按时间升序的像素点列。
inline std::vector<PixelPoint> map_series(const TimeSeries& s, Range xRange,
                                          Range yRange, long* skippedOut) {
  std::vector<PixelPoint> pts;
  const auto& t = s.times;
  const auto& v = s.values;
  const std::size_t n = std::min(t.size(), v.size());
  long skipped = 0;

  // 先收集有限点索引。
  std::vector<std::size_t> idx;
  idx.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (is_finite(t[i]) && is_finite(v[i]))
      idx.push_back(i);
    else
      ++skipped;
  }
  if (skippedOut != nullptr) *skippedOut += skipped;

  if (idx.empty()) return pts;

  if (static_cast<long>(idx.size()) <= kMaxSeriesPoints) {
    pts.reserve(idx.size());
    for (std::size_t i : idx) {
      pts.push_back({map_value(t[i], xRange, kMarginLeft,
                               kWidth - kMarginRight),
                     map_value(v[i], yRange, kHeight - kMarginBottom,
                               kMarginTop)});
    }
    return pts;
  }

  // 等宽桶：每桶保留首个最小值与首个最大值出现次序（保序保形）。
  const std::size_t buckets = static_cast<std::size_t>(kMaxSeriesPoints / 2);
  const std::size_t bucketSize =
      (idx.size() + buckets - 1) / buckets;  // 向上取整
  for (std::size_t b = 0; b < idx.size(); b += bucketSize) {
    const std::size_t end = std::min(b + bucketSize, idx.size());
    std::size_t iMin = idx[b], iMax = idx[b];
    for (std::size_t k = b; k < end; ++k) {
      const std::size_t i = idx[k];
      if (v[i] < v[iMin]) iMin = i;
      if (v[i] >= v[iMax]) iMax = i;
    }
    for (std::size_t i : {iMin, iMax}) {
      pts.push_back({map_value(t[i], xRange, kMarginLeft,
                               kWidth - kMarginRight),
                     map_value(v[i], yRange, kHeight - kMarginBottom,
                               kMarginTop)});
    }
  }
  // 桶内可能乱序（min/max 次序），按 time 排回升序。
  std::stable_sort(pts.begin(), pts.end(),
                   [](const PixelPoint& a, const PixelPoint& b) { return a.x < b.x; });
  return pts;
}

}  // namespace mcruntime::plot
