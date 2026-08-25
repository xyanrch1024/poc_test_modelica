# 契约: CSV 结果文件格式

适用于：生成程序输出（`<model>_result.csv`）、OpenModelica 参考
（转换后统一为本 schema）、回归比对器输入。

## 结构

```text
time,x,y
0.00000000000000000,1.00000000000000000,0.50000000000000000
...
```

| 规则 | 说明 |
|------|------|
| 表头 | 首行 `time,<var1>,<var2>,...`，变量顺序 = 源模型声明序（确定性） |
| 分隔符 | 半角逗号 `,`，无数千分位、无引号 |
| 数值格式 | `%.17g` 十进制定点/科学计数，区域设置无关（C locale） |
| 行序 | time 严格递增，含起点行与终点行 |
| 编码 | UTF-8 无 BOM，`\n` 换行 |

## 比对规则（tools/regression/compare.py 实现）

- 以生成程序的时间网格为基准网格，参考 CSV 在该网格上做线性插值取样。
- 逐变量、逐点计算偏差 `d = |a-b|`，通过条件：`d <= tol_abs + tol_rel*|b|`
  （默认 `tol_abs=1e-6, tol_rel=1e-4`，用例可覆盖）。
- 报告每个用例的最大绝对偏差及其发生时间（ValidationReport 契约）。

## 与 OpenModelica 参考的对接

omc `simulate(..., outputFormat="csv")` 产出的 CSV 含额外派生列；
入库 `references/` 前由脚本裁剪为上述 schema（保留 time + 声明变量列），
保证比对器只消费单一格式。
