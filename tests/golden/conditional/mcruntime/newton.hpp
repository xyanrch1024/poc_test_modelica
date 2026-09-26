#pragma once
// mcruntime：确定性 Newton-Raphson（前向有限差分雅可比）。
// 仅依赖 C++ 标准库；失败抛 std::runtime_error（与 RK4 发散同一退出路径）。
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace mcruntime {

struct NewtonOpts {
  double tol = 1e-10;
  int maxIter = 50;
  double fdEps = 1e-8;
};

// residual(x, r)：写入 r[0..n)，要求 ||r||_inf < tol。
// 成功时 x 为解；失败抛异常。
template <typename Residual>
void newton(Residual &&residual, double *x, size_t n, const NewtonOpts &opts = {}) {
  if (n == 0)
    return;
  std::vector<double> r(n), xtry(n), rtry(n), dx(n);
  std::vector<std::vector<double>> J(n, std::vector<double>(n));

  for (int iter = 0; iter < opts.maxIter; ++iter) {
    residual(x, r.data());
    double maxAbs = 0.0;
    for (size_t i = 0; i < n; ++i) {
      if (!std::isfinite(r[i])) {
        throw std::runtime_error("代数环求解发散：残差非有限");
      }
      maxAbs = std::max(maxAbs, std::fabs(r[i]));
    }
    if (maxAbs < opts.tol)
      return;

    // 前向差分雅可比
    for (size_t j = 0; j < n; ++j) {
      for (size_t k = 0; k < n; ++k)
        xtry[k] = x[k];
      const double h = opts.fdEps * (1.0 + std::fabs(x[j]));
      xtry[j] += h;
      residual(xtry.data(), rtry.data());
      for (size_t i = 0; i < n; ++i) {
        J[i][j] = (rtry[i] - r[i]) / h;
      }
    }

    // 高斯消元解 J dx = -r（部分主元）
    std::vector<std::vector<double>> A = J;
    for (size_t i = 0; i < n; ++i)
      dx[i] = -r[i];

    for (size_t col = 0; col < n; ++col) {
      size_t piv = col;
      double best = std::fabs(A[col][col]);
      for (size_t row = col + 1; row < n; ++row) {
        const double v = std::fabs(A[row][col]);
        if (v > best) {
          best = v;
          piv = row;
        }
      }
      if (best < 1e-18) {
        throw std::runtime_error("代数环求解失败：雅可比奇异");
      }
      if (piv != col) {
        std::swap(A[piv], A[col]);
        std::swap(dx[piv], dx[col]);
      }
      const double diag = A[col][col];
      for (size_t row = col + 1; row < n; ++row) {
        const double f = A[row][col] / diag;
        for (size_t k = col; k < n; ++k)
          A[row][k] -= f * A[col][k];
        dx[row] -= f * dx[col];
      }
    }
    for (size_t i = n; i-- > 0;) {
      double s = dx[i];
      for (size_t k = i + 1; k < n; ++k)
        s -= A[i][k] * dx[k];
      dx[i] = s / A[i][i];
    }

    for (size_t i = 0; i < n; ++i) {
      x[i] += dx[i];
      if (!std::isfinite(x[i])) {
        throw std::runtime_error("代数环求解发散：迭代值非有限");
      }
    }
  }
  throw std::runtime_error("代数环求解失败：超过最大迭代次数");
}

} // namespace mcruntime
