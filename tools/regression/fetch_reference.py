#!/usr/bin/env python3
"""从 OpenModelica 生成参考基线并裁剪为标准 CSV schema（contracts/csv-result-schema.md）。

前置：omc 在 PATH 中（OpenModelica ≥ 1.23）。用法：
    python3 tools/regression/fetch_reference.py examples/models/case_01_decay.mo

流程：
  1. 解析模型文件名与 experiment 注释（StartTime/StopTime/Interval，缺省同 omc 惯例）；
  2. 生成临时 .mos 脚本：loadFile + simulate(..., outputFormat="csv")；
  3. 调用 omc 执行；
  4. 将 <Model>_res.csv 裁剪为 time + 源声明变量列，写入 references/<Case>_res.csv。
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_experiment(source: str) -> dict:
    exp = {"StartTime": 0.0, "StopTime": None, "Interval": None}
    m = re.search(r"experiment\s*\(([^)]*)\)", source)
    if not m:
        raise ValueError("缺少 experiment 注释")
    for item in m.group(1).split(","):
        if "=" not in item:
            continue
        key, val = (s.strip() for s in item.split("=", 1))
        if key in exp:
            exp[key] = float(val)
    if exp["StopTime"] is None or exp["Interval"] is None:
        raise ValueError("experiment 缺少 StopTime 或 Interval")
    return exp


def declared_variables(source: str) -> list:
    """提取 Variable 类别的声明名（排除 constant/parameter 行；der 目标也算状态）。"""
    names = []
    for m in re.finditer(
        r"^\s*(?:(?:constant|parameter)\s+)?(Real|Integer|Boolean)\s+([A-Za-z_]\w*)",
        source,
        re.M,
    ):
        kind, name = m.group(1), m.group(2)
        prefix = m.group(0)
        if "constant" in prefix or "parameter" in prefix:
            continue
        names.append(name)
    return names


def main(argv: list) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("model", help=".mo 模型文件")
    ap.add_argument("--out-dir", default="references")
    args = ap.parse_args()

    if shutil.which("omc") is None:
        print("错误: PATH 中未找到 omc（需安装 OpenModelica ≥1.23）", file=sys.stderr)
        return 2

    model_path = Path(args.model).resolve()
    source = model_path.read_text(encoding="utf-8")
    case = model_path.stem
    exp = parse_experiment(source)
    intervals = max(1, round((exp["StopTime"] - exp["StartTime"]) / exp["Interval"]))
    variables = declared_variables(source)

    with tempfile.TemporaryDirectory() as td:
        mos = Path(td) / f"gen_{case}.mos"
        mos.write_text(
            f'loadFile("{model_path}");\n'
            f'simulate({case}, startTime={exp["StartTime"]}, stopTime={exp["StopTime"]}, '
            f'numberOfIntervals={intervals}, outputFormat="csv");\n'
            "exit();\n",
            encoding="utf-8",
        )
        proc = subprocess.run(["omc", str(mos)], cwd=td, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"omc 失败:\n{proc.stderr}", file=sys.stderr)
            return 3
        raw = Path(td) / f"{case}_res.csv"
        if not raw.exists():
            print(f"omc 未产出 {raw.name}:\n{proc.stdout[-800:]}", file=sys.stderr)
            return 3

        lines = [ln.strip() for ln in raw.read_text().splitlines() if ln.strip()]
        header = lines[0].split(",")
        keep_idx = [header.index("time")] + [
            i for i, h in enumerate(header) if h in variables and i != header.index("time")
        ]
        keep_names = ["time"] + [header[i] for i in keep_idx[1:]]
        out_lines = [",".join(keep_names)]
        for ln in lines[1:]:
            cells = ln.split(",")
            out_lines.append(",".join(cells[i] for i in keep_idx))

        out_dir = Path(args.out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        out_path = out_dir / f"{case}_res.csv"
        out_path.write_text("\n".join(out_lines) + "\n", encoding="utf-8")

    print(f"已生成参考: {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
