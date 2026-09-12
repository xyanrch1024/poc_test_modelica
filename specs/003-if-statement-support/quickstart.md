# Quickstart: if 条件构造端到端验证指南

**Feature**: 003-if-statement-support
目标：证明"含条件的 Modelica → C++ → 运行 → CSV"全链路与既有 001 能力同构，且
条件语义按规格求值（clear step-boundary discrete semantics, no events）。
实现细节见 [plan.md](./plan.md)、[data-model.md](./data-model.md) 与 [contracts/](./contracts/)。

## 前置条件

```bash
./build/src/modelicac --version # 已有 001 交付物；本特性开发期用 build 中对应二进制
cmake --version             # ≥ 3.24
python3 --version           # ≥ 3.11
omc --version               # 仅生成参考基线与场景 A 可选比对时需要
```

## 场景 A: 条件表达式模型（用户故事 1 扩展）

1. **示例模型** `examples/models/case_21_cond_expr.mo`（解析可核对的折线/冲击波）：

   ```modelica
   model Case21CondExpr
     parameter Real y0 = 1.0;
     parameter Real slope = 2.0;
     Real y;
     Real x(start = 0, fixed = true);
   equation
     y = if x > 1 then y0 + slope * (x - 1) else y0 + x;
     der(x) = 1;
     annotation(experiment(StartTime = 0, StopTime = 3, Interval = 0.01));
   end Case21CondExpr;
   ```

2. **转换**：
   ```bash
   ./build/src/modelicac translate examples/models/case_21_cond_expr.mo -o /tmp/cond_gen
   ```
   ✅ exit 0；产物含 CMakeLists.txt 与模型源文件（条件内联为三元，无语句级 if 块）。

3. **构建并运行**：
   ```bash
   cmake -S /tmp/cond_gen -B /tmp/cond_gen/build && cmake --build /tmp/cond_gen/build
   cd /tmp && ./cond_gen/build/Case21CondExpr
   ```
   ✅ exit 0；`Case21CondExpr_result.csv`；表头 `time,y,x`（声明序）；
   切换发生在 x 越过 1 的 RK4 步内（阶段采样），故该步结束值可能有 ≤ h/6 的过冲。

4. **参考比对（可选）**：`omc references/gen_reference_if.mos` 生成
   `references/case_21_cond_expr_res.csv` 后：
   ```bash
   python3 tools/regression/run_regression.py examples/models/references_if.cases
   ```
   ✅ `SUMMARY: N/N PASS`。注意运行期切换的容差以步边界语义收敛（见 research §5），
   常量条件用例（场景 B）应与 omc 逐点一致。

## 场景 B: 常量条件回归锚点

`case_22_cond_const.mo`：条件只引用 constant/parameter → 整个仿真期同一分支，
生成值应与 omc 参考逐点一致（无事件语义差异）。

```modelica
model Case22CondConst
  parameter Real x = 5;
  Real y;
  Real z;
equation
  y = if x > 2 then x * 1.5 else x;   // 恒定走 then 分支
  z = if x > 0 then y else 1;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));
end Case22CondConst;
```

✔ 该用例同时覆盖"常量 if 表达式"与"if 表达式嵌套"。

## 场景 C: 条件方程/状态分支/嵌套方程

`case_23_cond_eq.mo`：代数条件方程（二分支，US1） + 状态量条件导数 + 嵌套 if 表达式。
`case_24_cond_elseif.mo`（US2）覆盖 elseif 多分支链：

```modelica
model Case23CondEq
  Real q;
  Real r;
  Real s(start = 0, fixed = true);
equation
  if s < 1 then
    q = 2 * s;
  else
    q = s + 1;
  end if;
  r = if s < 1 then q else -q;   // 嵌套 if 表达式
  der(s) = if s < 1 then 1 else 2;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.01));
end Case23CondEq;
```

✅ 语义自校验点：s 越过 1 时 q 切到 `s+1` 支、der(s) 切到 2；切换在越过点的
RK4 步内按阶段采样生效（无事件精化），该步结束值与理想值偏差 ≤ h/6。
（elseif 三/多分段样本见 US2 用例 `case_24_cond_elseif.mo`。）

## 场景 D: 诊断路径（用户故事 3 扩展）

非法输入必须拒绝并给出位置与 MC 码（FR-006）；合法输入 exit 0：

```bash
printf 'model M\n  Real y;\n  Real z;\n  Boolean b;\nequation\n  z = 2;\n  b = z > 0;\n  y = if b then 1 else z;\n  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));\nend M;\n' > /tmp/ok.mo
./build/src/modelicac translate /tmp/ok.mo -o /tmp/ok_gen     # exit 0（Boolean 变量条件合法）

printf 'model A\n  Real y;\nequation\n  y = if y > 1 then 2;\n  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));\nend A;\n' > /tmp/nodiv.mo
./build/src/modelicac translate /tmp/nodiv.mo    # if 表达式缺 else → 解析错误 "期望 else，实际为 ..."

printf 'model B\n  Real x;\n  Real y;\n  Real s(start = 0, fixed = true);\nequation\n  der(s) = 1;\n  if s > 0 then\n    x = 1;\n  else\n    y = 2;\n  end if;\n  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));\nend B;\n' > /tmp/badbal.mo
./build/src/modelicac translate /tmp/badbal.mo    # 分支 LHS 不同未知量 x vs y → MC0305

printf 'model C\n  Real y;\n  Real x(start = 0, fixed = true);\nequation\n  der(x) = 1;\n  y = if x then 1 else -1;\n  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));\nend C;\n' > /tmp/badcond.mo
./build/src/modelicac translate /tmp/badcond.mo    # 条件非布尔（x 为 Real）→ MC0204
```

✅ 非法输入各自 stderr 带 `[error] file:line:col: … [MC0201/0305/0204]`，退出码 2，
不产生输出目录；合法输入 exit 0 正常产出工程。

## 场景 E: 测试全绿（宪法 II 门禁）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
```

✅ 预期：既有用例保持全绿，新增 unit/golden/integration 用例全部通过，
无任何 `if` 相关回归（截至 T037 前为 96/96 通过）。

## 验证矩阵（本文四场景 ↔ 规格条目）

| 场景 | 覆盖 | Spec 条目 |
|------|------|-----------|
| A | 条件表达式全链路（doc-to-CSV） | FR-001/010、SC-001 |
| B | 常量条件逐点正确性 | FR-010、SC-002 |
| C | 条件方程+elseif+状态分支 | FR-002/003、SC-001 |
| D | 非法输入诊断与 MC 码 | FR-004/006、SC-003 |
| E | 无回归 + 新用例 | FR-009、SC-004 |