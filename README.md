# pocmodelica — Modelica → C++ 编译器 (POC)

将首期 Modelica 子集（连续时间 ODE 模型，见
[spec](specs/001-modelica-cpp-compiler/spec.md) FR-004）翻译为可构建、可运行、
确定性输出的 C++ 仿真程序。

## 工具链基线（固定版本，宪法原则 V）

| 组件 | 版本要求 |
|------|----------|
| GCC / Clang | ≥12 / ≥15（`-std=c++20`） |
| CMake | ≥3.24 |
| GoogleTest | v1.14.0（FetchContent 自动拉取） |
| clang-format / clang-tidy | 17.x |
| Python / pytest | ≥3.11 / ≥8（`tools/regression/requirements.txt`） |
| OpenModelica（参考基准生成） | ≥1.23（仅 `references/` 基线再生成时需要） |

## 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 快速使用

```bash
./build/modelicac translate examples/models/cooling.mo -o /tmp/cooling_gen
cmake -S /tmp/cooling_gen -B /tmp/cooling_gen/build && cmake --build /tmp/cooling_gen/build
/tmp/cooling_gen/build/Cooling   # 生成 Cooling_result.csv（零命令行参数）
```

## 条件构造（if）支持

首期子集支持以下 `if` 构造（详见
[if 构造契约](specs/003-if-statement-support/contracts/if-construct-contract.md)）：

- 条件表达式：`y = if cond then a else b;`（可嵌套、可作子表达式）。
- 条件方程：`if cond then ... elseif cond2 then ... else ... end if;`
  每分支须恰好一条方程、各分支求解同一未知量（变量或 `der(变量)`）。
- `elseif` 链与嵌套 `if` 均可；运行期条件的条件方程必须含 `else`。
- 条件仅引用 `constant`/`parameter` 时做编译期静态折叠（缺 `else` 仅此时允许）。

### 离散求值语义（FR-010）

条件布尔在每次右端项/`deriv` 求值点（含 RK4 各阶段）按当前状态重新采样：
分支切换在**首次使条件为真的求值点所在步内**生效，不做事件/过零精化，切换处局部
误差有界（≤ 步长量级 `h/6`）；CSV 每行反映该步结束时的分支取值。

### 相关诊断码

| 码 | 含义 |
|----|------|
| MC0203 | `der(...)` 出现在方程左侧之外（含条件表达式内） |
| MC0204 | `if` 条件不是布尔表达式 |
| MC0205 | `if` 表达式两分支类型不一致（数值/布尔混用） |
| MC0305 | 条件方程分支不平衡（非恰一方程、未知量不一致、运行期条件缺 `else`） |

## 文档索引

- 规格: specs/001-modelica-cpp-compiler/spec.md
- 计划: specs/001-modelica-cpp-compiler/plan.md
- 任务: specs/001-modelica-cpp-compiler/tasks.md
- 契约: specs/001-modelica-cpp-compiler/contracts/
