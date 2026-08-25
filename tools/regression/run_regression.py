#!/usr/bin/env python3
"""批量回归驱动（用户故事 2 / FR-010 / SC-002）。

清单格式（references.cases，'#' 注释）：
    <CaseName> <modelPath> <referenceCsv> [tol_rel] [tol_abs]

对每个用例执行 转换 → 构建 → 运行 → compare.py 比对：
  - PASS/FAIL 行含量化最大偏差；
  - 参考文件缺失时报 SKIP(no-reference)——不失败，便于 omc 缺失环境使用；
  - 任一 FAIL → 进程退出非零；末行输出 SUMMARY: <pass>/<executed> PASS。
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare import DEFAULT_TOL_ABS, DEFAULT_TOL_REL, compare  # noqa: E402


def parse_cases(manifest: Path):
    cases = []
    for raw in manifest.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) < 3:
            raise ValueError(f"清单行格式错误: {raw}")
        name, model, ref = parts[:3]
        tol_rel = float(parts[3]) if len(parts) > 3 else DEFAULT_TOL_REL
        tol_abs = float(parts[4]) if len(parts) > 4 else DEFAULT_TOL_ABS
        cases.append((name, model, ref, tol_rel, tol_abs))
    return cases


def build_and_run(binary: str, model: Path, work: Path) -> tuple:
    proj = work / f"{model.stem}_gen"
    run_dir = work / f"{model.stem}_run"
    run_dir.mkdir(parents=True, exist_ok=True)

    r = subprocess.run([binary, "translate", str(model), "-o", str(proj)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return False, f"translate 失败: {r.stderr.strip()[:200]}"

    for cmd in (["cmake", "-S", str(proj), "-B", str(proj / "build"),
                 "-DCMAKE_BUILD_TYPE=Release"],
                ["cmake", "--build", str(proj / "build"), "--parallel"]):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            return False, f"构建失败 ({cmd[0]}): {r.stderr.strip()[-200:]}"

    exe = proj / "build" / model.stem
    if not exe.exists():
        candidates = list((proj / "build").glob(model.stem))
        exe = candidates[0] if candidates else exe
    r = subprocess.run([str(exe)], cwd=run_dir, capture_output=True, text=True)
    if r.returncode != 0:
        return False, f"运行失败 exit={r.returncode}: {r.stderr.strip()[:200]}"

    result_csv = run_dir / f"{model.stem}_result.csv"
    if not result_csv.exists():
        return False, f"未找到结果文件 {result_csv.name}"
    return True, str(result_csv)


def main(argv: list) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest", nargs="?", default="examples/models/references.cases")
    ap.add_argument("--binary", default=os.environ.get("MODELICAC_BIN", "./build/src/modelicac"))
    args = ap.parse_args()

    manifest = Path(args.manifest)
    cases = parse_cases(manifest)

    passed = executed = failed = skipped = 0
    with tempfile.TemporaryDirectory() as td:
        work = Path(td)
        for name, model, ref, tol_rel, tol_abs in cases:
            ref_path = Path(ref)
            if not ref_path.exists():
                print(f"{name}: SKIP(no-reference)")
                skipped += 1
                continue
            ok_run, payload = build_and_run(args.binary, Path(model), work)
            if not ok_run:
                print(f"{name}: FAIL {payload}")
                failed += 1
                executed += 1
                continue
            try:
                result = compare(payload, ref, None, tol_rel, tol_abs)
            except (AssertionError, KeyError, ValueError) as exc:
                print(f"{name}: FAIL {exc}")
                failed += 1
            else:
                print(f"{name}: PASS max_abs_dev={result['max_abs_dev']:.6g} "
                      f"at t={result['max_dev_time']:.6g}")
                passed += 1
            executed += 1

    print(f"SUMMARY: {passed}/{executed} PASS"
          + (f" (skipped={skipped})" if skipped else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
