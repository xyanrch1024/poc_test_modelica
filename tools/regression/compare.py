#!/usr/bin/env python3
"""CSV 结果比对器（contracts/csv-result-schema.md）。

以"实际输出"的时间网格为基准，将参考轨迹线性插值到该网格后逐变量逐点比较：
    |a - b| <= tol_abs + tol_rel * |b|
报告最大绝对偏差及其发生时间。
"""

from __future__ import annotations

import argparse
import sys
from typing import Dict, List, Optional, Tuple

DEFAULT_TOL_REL = 1e-4
DEFAULT_TOL_ABS = 1e-6


def load_csv(path: str) -> Tuple[List[float], Dict[str, List[float]]]:
    """返回 (time 列, {列名: 值列})。表头必须含 time。"""
    with open(path, "r", encoding="utf-8") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    if not lines:
        raise ValueError(f"空文件: {path}")
    header = lines[0].split(",")
    if header[0] != "time":
        raise ValueError(f"表头首列必须是 time: {path}")
    times: List[float] = []
    cols: Dict[str, List[float]] = {name: [] for name in header[1:]}
    prev_t = None
    for ln in lines[1:]:
        cells = ln.split(",")
        if len(cells) != len(header):
            raise ValueError(f"列数不一致 @ {path}: {ln}")
        t = float(cells[0])
        if prev_t is not None and t <= prev_t:
            raise ValueError(f"time 非严格递增 @ {path}")
        prev_t = t
        times.append(t)
        for name, cell in zip(header[1:], cells[1:]):
            cols[name].append(float(cell))
    return times, cols


def interp(times_ref: List[float], values_ref: List[float], t: float) -> float:
    """参考网格上的线性插值；t 越出范围时取最近端点值。"""
    if t <= times_ref[0]:
        return values_ref[0]
    if t >= times_ref[-1]:
        return values_ref[-1]
    lo, hi = 0, len(times_ref) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if times_ref[mid] <= t:
            lo = mid
        else:
            hi = mid
    t0, t1 = times_ref[lo], times_ref[hi]
    v0, v1 = values_ref[lo], values_ref[hi]
    if t1 == t0:
        return v1
    frac = (t - t0) / (t1 - t0)
    return v0 + frac * (v1 - v0)


def compare(
    actual_path: str,
    reference_path: str,
    columns: Optional[List[str]] = None,
    tol_rel: float = DEFAULT_TOL_REL,
    tol_abs: float = DEFAULT_TOL_ABS,
) -> Dict[str, object]:
    """比对两个结果文件。columns 为空时使用实际文件的全部非 time 列。"""
    t_act, cols_act = load_csv(actual_path)
    t_ref, cols_ref = load_csv(reference_path)
    if not t_act:
        raise ValueError(f"实际结果无数据行: {actual_path}")

    names = columns if columns else list(cols_act.keys())
    missing = [n for n in names if n not in cols_act or n not in cols_ref]
    if missing:
        raise KeyError(f"缺失比对列: {missing}")

    max_dev = -1.0
    max_dev_time = float("nan")
    per_column = {}
    for i, t in enumerate(t_act):
        for n in names:
            b = interp(t_ref, cols_ref[n], t)
            a = cols_act[n][i]
            dev = abs(a - b)
            limit = tol_abs + tol_rel * abs(b)
            if dev > limit:
                raise AssertionError(
                    f"超出容差: 列 {n} t={t:.17g} 实际={a:.17g} 参考={b:.17g} "
                    f"(偏差 {dev:.3g} > 上限 {limit:.3g})"
                )
            if dev > max_dev:
                max_dev = dev
                max_dev_time = t
                per_column[n] = (dev, t)

    ok = max_dev >= 0.0  # 有数据即视为完成比较（超差点已在上方抛出）
    return {
        "ok": bool(ok),
        "max_abs_dev": max(max_dev, 0.0),
        "max_dev_time": max_dev_time,
        "per_column": per_column,
        "points": len(t_act),
    }


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser(description="Modelica→C++ 结果轨迹比对器")
    ap.add_argument("actual", help="生成程序输出的 CSV")
    ap.add_argument("reference", help="参考基准 CSV（OpenModelica 裁剪版）")
    ap.add_argument("--tol-rel", type=float, default=DEFAULT_TOL_REL)
    ap.add_argument("--tol-abs", type=float, default=DEFAULT_TOL_ABS)
    args = ap.parse_args()
    try:
        result = compare(args.actual, args.reference, None, args.tol_rel, args.tol_abs)
    except (AssertionError, KeyError, ValueError) as exc:
        print(f"FAIL {exc}")
        return 1
    print(
        f"PASS points={result['points']} "
        f"max_abs_dev={result['max_abs_dev']:.6g} at t={result['max_dev_time']:.6g}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
