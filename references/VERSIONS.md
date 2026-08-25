# 参考基线版本记录（宪法原则 V：可复现性）

| 项目 | 值 |
|------|-----|
| 参考生成工具 | OpenModelica omc ≥ 1.23 |
| 输出格式 | `simulate(..., outputFormat="csv")` 后按 contracts/csv-result-schema.md 裁剪 |
| 当前状态 | **尚无入库基线** —— 开发环境未安装 OpenModelica |

## 生成方法

```bash
# 单个模型（推荐，自动裁剪为 time + 声明变量列）：
python3 tools/regression/fetch_reference.py examples/models/case_01_decay.mo

# 批量：
for f in examples/models/case_*.mo; do python3 tools/regression/fetch_reference.py "$f"; done
```

## 约定

- 基线一经入库即冻结；再生成必须换用新文件名或在 commit message 说明理由。
- 每次生成后在表格追加一行：`<日期> | <omc 版本> | <受影响用例>`。
- 缺失基线的用例在 run_regression.py 中记为 SKIP(no-reference)，不阻塞套件。
