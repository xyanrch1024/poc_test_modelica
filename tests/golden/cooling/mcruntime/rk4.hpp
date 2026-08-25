#pragma once
// mcruntime：供 modelicac 生成代码包含的固定步长 RK4 积分器。
// 仅依赖 C++ 标准库（contracts/generated-program-contract.md）。
// 逐步确定性：无自适应步长、无并行、无平台相关浮点行为。
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace mcruntime {

namespace detail {
inline std::string formatTime(double t) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", t);
  return buf;
}
} // namespace detail

// 经典四阶 Runge-Kutta 单步推进。
// rhs(t, y, dydt)：按 y 分量次序计算导数；y 与 dydt 等长。
template <typename F>
void rk4_step(double h, double &t, std::vector<double> &y, F &&rhs) {
  const size_t n = y.size();
  std::vector<double> k1(n), k2(n), k3(n), k4(n), tmp(n);

  rhs(t, y.data(), k1.data());
  for (size_t i = 0; i < n; ++i)
    tmp[i] = y[i] + 0.5 * h * k1[i];
  rhs(t + 0.5 * h, tmp.data(), k2.data());
  for (size_t i = 0; i < n; ++i)
    tmp[i] = y[i] + 0.5 * h * k2[i];
  rhs(t + 0.5 * h, tmp.data(), k3.data());
  for (size_t i = 0; i < n; ++i)
    tmp[i] = y[i] + h * k3[i];
  rhs(t + h, tmp.data(), k4.data());

  for (size_t i = 0; i < n; ++i) {
    y[i] += (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    if (!std::isfinite(y[i])) {
      throw std::runtime_error("数值发散：t=" + detail::formatTime(t + h) + " 出现非有限状态值");
    }
  }
  t += h;
}

} // namespace mcruntime
