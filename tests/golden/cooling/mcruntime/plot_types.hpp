#pragma once
// mcruntime::plot —— 内嵌绘图公共类型（specs/002-simulation-plot/data-model.md）。
// 仅依赖 C++ 标准库（FR-010）。版面/调色板为编译期常量（宪法 IV/V）。
#include <cstdint>
#include <string>
#include <vector>

namespace mcruntime::plot {

// ---- 版面常量（contracts/png-output-contract.md）----
constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr int kMarginLeft = 72;
constexpr int kMarginRight = 24;
constexpr int kMarginTop = 48;
constexpr int kMarginBottom = 44;
constexpr int kPlotW = kWidth - kMarginLeft - kMarginRight;
constexpr int kPlotH = kHeight - kMarginTop - kMarginBottom;
constexpr int kMaxSeriesPoints = kPlotW * 2;  // 超过则桶降采样

struct Rgb {
  std::uint8_t r, g, b;
};
inline constexpr bool operator==(const Rgb &a, const Rgb &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}
inline constexpr bool operator!=(const Rgb &a, const Rgb &b) { return !(a == b); }
inline constexpr Rgb kPalette[8] = {
    {31, 119, 180},  {255, 127, 14},  {44, 160, 44},   {214, 39, 40},
    {148, 103, 189}, {140, 86, 75},   {127, 127, 127}, {188, 189, 34},
};

// ---- 绘图请求（来自 --plot 系列开关）----
struct PlotRequest {
  bool enabled = false;
  std::vector<std::string> columns;  // 空 = 全部变量列
  std::string outputPath;            // 空 = <模型名>_result.png
};

// ---- 单变量时间序列（与落盘 CSV 同源同 schema）----
struct TimeSeries {
  std::string name;
  std::vector<double> times;
  std::vector<double> values;
};

// ---- 渲染统计（结束摘要依据，FR-008/契约摘要行）----
struct RenderStats {
  bool ok = false;
  long skippedPoints = 0;      // 剔除的非有限点总数
  int drawnSeries = 0;         // 成功绘制的序列数
  int skippedSeries = 0;       // 全坏点放弃的序列数
  std::string outputPath;      // 成功时的实际写出路径
  std::string skipReason;      // 放弃原因短语（摘要行括号内）
  std::string detail;          // 面向 stderr 的详细诊断（可为空）
};

}  // namespace mcruntime::plot
