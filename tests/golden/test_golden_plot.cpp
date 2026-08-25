#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "plot_render.hpp"

namespace {

using namespace mcruntime::plot;

// PNG 金样：固定合成序列（Hello 等价数据）→ 逐字节比对基线（FR-006）。
// 基线再生成方式：临时在测试中置 regenerate=true 运行一次后拷贝输出；
// 或直接用 Hello 生成程序 --plot 的产物替换 tests/golden/hello_plot/baseline.png。
std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return "";
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

TEST(GoldenPlot, MatchesBaselineByteForByte) {
  // 与 example/hello.mo 完全一致的数据与命名
  std::vector<TimeSeries> series;
  TimeSeries x;
  x.name = "x";
  for (int i = 0; i <= 10; ++i) {
    x.times.push_back(i * 0.1);
    x.values.push_back(i * 0.1);
  }
  series.push_back(x);

  PlotRequest req;
  req.enabled = true;

  Framebuffer fb;
  RenderStats stats;
  ASSERT_TRUE(render_to_framebuffer("Hello", series, req, fb, stats));

  auto bytes = encode_png(kWidth, kHeight, fb.raw());
  const std::string actual(reinterpret_cast<const char*>(bytes.data()), bytes.size());

  constexpr bool kRegenerate = false;  // 基线损坏/有意变更时置 true 跑一次
  const std::string baselinePath = std::string(GOLDEN_DIR) + "/hello_plot/baseline.png";
  if (kRegenerate) {
    std::ofstream out(baselinePath, std::ios::binary);
    out.write(actual.data(), static_cast<std::streamsize>(actual.size()));
  }
  const std::string expected = readFile(baselinePath);
  ASSERT_FALSE(expected.empty()) << "缺少基线: " << baselinePath;
  EXPECT_EQ(actual, expected) << "PNG 金样不一致";

  // 同帧缓冲二次编码逐字节一致
  auto again = encode_png(kWidth, kHeight, fb.raw());
  EXPECT_EQ(bytes, again);
}

}  // namespace
