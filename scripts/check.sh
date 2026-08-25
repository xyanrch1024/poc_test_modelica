#!/usr/bin/env bash
# 一键质量门禁（宪法 I/II：风格 + 静态检查 + 全量测试）。
set -euo pipefail
cd "$(dirname "$0")/.."

if command -v clang-format >/dev/null 2>&1; then
  echo "== clang-format =="
  # 金样基线（tests/golden）是冻结产物，禁止被格式化工具改写。
  find src tests -path tests/golden -prune -o \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print | while read -r f; do
    clang-format --dry-run --Werror "$f"
  done
else
  echo "(clang-format 未安装，跳过)"
fi

if command -v clang-tidy >/dev/null 2>&1; then
  echo "== clang-tidy (抽样核心源文件) =="
  clang-tidy -p build src/parser/parser.cpp src/codegen/codegen.cpp --quiet
else
  echo "(clang-tidy 未安装，跳过)"
fi

echo "== 构建 =="
cmake --build build --parallel

echo "== 测试 =="
ctest --test-dir build --output-on-failure

echo "== 回归比对器单测 =="
python3 -m pytest tools/regression/test_compare.py -q

echo "门禁通过 ✔"
