"""compare.py 的 pytest 单元测试（任务 T023，先于实现编写并确认失败）。"""

from __future__ import annotations

import pytest

from compare import compare, interp, load_csv


@pytest.fixture()
def actual_csv(tmp_path):
    """实际输出：t = 0, 0.5, 1；x(t)=t（线性），y 常量 1。"""
    p = tmp_path / "actual.csv"
    p.write_text("time,x,y\n0,0,1\n0.5,0.5,1\n1,1,1\n")
    return str(p)


@pytest.fixture()
def reference_csv(tmp_path):
    """参考：更密网格上的同一线性轨迹 x=t、y=1。"""
    rows = ["time,x,y"]
    for i in range(11):
        t = i / 10
        rows.append(f"{t},{t},1")
    p = tmp_path / "reference.csv"
    p.write_text("\n".join(rows) + "\n")
    return str(p)


class TestLoadCsv:
    def test_loads_columns_and_times(self, actual_csv):
        times, cols = load_csv(actual_csv)
        assert times == [0.0, 0.5, 1.0]
        assert cols["x"] == [0.0, 0.5, 1.0]
        assert cols["y"] == [1.0, 1.0, 1.0]

    def test_rejects_missing_time_header(self, tmp_path):
        p = tmp_path / "bad.csv"
        p.write_text("t,x\n0,1\n")
        with pytest.raises(ValueError):
            load_csv(str(p))

    def test_rejects_non_increasing_time(self, tmp_path):
        p = tmp_path / "bad2.csv"
        p.write_text("time,x\n0,1\n0,1\n")
        with pytest.raises(ValueError):
            load_csv(str(p))


class TestInterp:
    def test_midpoint_linear(self):
        assert interp([0.0, 1.0], [0.0, 10.0], 0.5) == pytest.approx(5.0)

    def test_clamps_outside_range(self):
        assert interp([0.0, 1.0], [2.0, 4.0], -5.0) == 2.0
        assert interp([0.0, 1.0], [2.0, 4.0], 9.0) == 4.0


class TestCompare:
    def test_identical_trajectories_pass(self, actual_csv, reference_csv):
        r = compare(actual_csv, reference_csv)
        assert r["ok"] is True
        assert r["max_abs_dev"] == pytest.approx(0.0)

    def test_within_tolerance_passes(self, actual_csv, tmp_path):
        ref = tmp_path / "ref_small.csv"
        # y 列整体偏移 1e-7 < tol_abs
        ref.write_text("time,y\n0,1.0000001\n1,1.0000001\n")
        r = compare(actual_csv, str(ref), columns=["y"], tol_abs=1e-6)
        assert r["ok"] is True

    def test_exceeding_tolerance_raises(self, actual_csv, tmp_path):
        ref = tmp_path / "ref_big.csv"
        ref.write_text("time,x\n0,0\n1,1.1\n")  # 终点偏差 ~0.1 >> 容差
        with pytest.raises(AssertionError):
            compare(actual_csv, str(ref), columns=["x"])

    def test_relative_tolerance_scales_with_reference(self, actual_csv, tmp_path):
        # 参考值 1000，偏差 0.05：相对容差 1e-4 → 上限 0.1+abs → 通过
        act = tmp_path / "act_big.csv"
        act.write_text("time,v\n0,1000.05\n")
        ref = tmp_path / "ref_bigv.csv"
        ref.write_text("time,v\n0,1000\n")
        r = compare(str(act), str(ref), columns=["v"])
        assert r["ok"] is True

    def test_missing_column_is_key_error(self, actual_csv, reference_csv):
        with pytest.raises(KeyError):
            compare(actual_csv, reference_csv, columns=["nope"])

    def test_report_contains_max_dev_and_time(self, actual_csv, tmp_path):
        ref = tmp_path / "ref_lin.csv"
        ref.write_text("time,x\n0,0\n0.5,0.52\n1,1\n")  # 最大偏差 0.02 @ t=0.5
        r = compare(actual_csv, str(ref), columns=["x"], tol_rel=0, tol_abs=0.1)
        assert r["max_abs_dev"] == pytest.approx(0.02)
        assert r["max_dev_time"] == pytest.approx(0.5)
