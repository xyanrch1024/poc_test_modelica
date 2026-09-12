# Implementation Plan: 支持 if 语句（条件方程与条件表达式）

**Branch**: `003-if-statement-support` | **Date**: 2026-09-12 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/003-if-statement-support/spec.md`

## Summary

扩展 Modelica 子集编译器，支持 `if` 条件表达式与条件方程：
- **条件表达式**：`y = if c then a else b`（`else` 必选，其类型与 then 分支一致）。
- **条件方程**：`if c then <eq>; {elseif c then <eq>;} else <eq>; end if;`
  —— 各分支求解**同一未知量**且每分支恰一个方程（Modelica 规范分支平衡约束的达标形式），
  使"方程数恒定"的单赋值规则成立。
- **求值语义**（规格 Clarifications 已确认）：条件允许依赖运行期变量，每次输出步按布尔求值，
  切换在步边界生效；无事件检测。

技术路线：在现有流水线上扩展五个模块——
文法（array 现有 EBNF 增加 `if-expression` 到 `primary`、`if-equation` 到 `equation`）、
AST（新增 `Expr::If` 与 `Equation` 条件变体）、语义（条件类型检查、分支平衡/同未知量校验、
代数依赖图按分支并集建边）、计划（条件方程归约为 `v.x = c ? a : b` 的赋值，与代码生成解耦）、
代码生成（`if` 表达式→C++ 三元运算符；条件方程→三元赋值直接内联进 `compute_algebraic/deriv`）。

因分支单一方程/单一未知量限制，条件方程在代码生成层**归约为条件表达式**，
无需生成语句级 if/else 块，保持生成程序结构与现有基线一致（确定性、无新运行时依赖）。

## Technical Context

**Language/Version**: C++20（GCC ≥12 / Clang ≥15）；生成程序仅依赖 C++ 标准库

**Primary Dependencies**: GoogleTest v1.14.0（FetchContent）、CMake ≥3.24；
参考基准 OpenModelica ≥1.23（omc，仅需再生成 `references/` 基线时）

**Storage**: N/A（输入 .mo、输出 C++ 工程目录与 CSV/源码；无持久化数据）

**Testing**: GoogleTest 单元测试（lexer/parser/语义/plan/codegen）× 金样 × CTest 集成；
pytest 覆盖回归比对器；clang-format/clang-tidy 静态检查门禁

**Target Platform**: Linux x86-64（WSL Ubuntu）；生成代码仅标准库保持可移植

**Project Type**: compiler/cli（单一仓库、单一主项目，沿用 001 布局）

**Performance Goals**: ≤500 行模型端到端转换 <10s 不回退（CTest Performance 用例维持）；
含条件嵌套的模型在既有规模内无感知劣化

**Constraints**: 确定性输出（FR-007）；诊断必含 file:line（FR-006）；生成程序零参数运行（FR-003）；
第三方依赖版本固定（FR-002）；不引入事件检测/求解器改动（Clarification Q2）

**Scale/Scope**: 新代码预计约 400–700 行：parser 两个新产生式 + AST 节点 + 语义类型判断/
分支校验 + codegen 表达式/方程两类发射；新增 ≥5 个代表性验证模型（SC-001）

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| 原则 | 设计落点 | 状态 |
|------|----------|------|
| I. 代码质量优先 | 变量"是否为布尔表达式"的类型判断与分支校验复用语义模块单测；条件表达式/方程共用一个归约逻辑避免重复；clang 门禁沿用 | ✅ PASS |
| II. 单元测试全覆盖 | 每个新产生式至少一个解析/语义/金样用例（FR-009）；非法分支（不同未知量/非布尔条件/缺 else）附回归用例 | ✅ PASS |
| III. 仿真结果可验证 | 条件模型与解析值/OpenModelica 参考比对；常量条件逐点一致，运行期条件按既定容差（SC-002） | ✅ PASS |
| IV. 简单性优先 | 分支限定"单方程单未知量"→ 条件方程代码生成归约为三元表达式，不引入语句级 if/else 与块级依赖分析 | ✅ PASS |
| V. 可复现性 | 生成仍为逐字节确定（有序遍历、`%.17g`）；步边界离散求值语义固定并随文档发布 | ✅ PASS |

**Gate 结论**: 无违规项，进入 Phase 0。

## Project Structure

### Documentation (this feature)

```text
specs/003-if-statement-support/
├── plan.md              # 本文件
├── research.md          # Phase 0 输出：技术决策记录
├── data-model.md        # Phase 1 输出：内部数据模型与归约规则
├── quickstart.md        # Phase 1 输出：端到端验证指南
├── contracts/           # Phase 1 输出：外部契约
│   ├── if-construct-contract.md   # if 文法扩展 + 分支平衡规则 + 新增 MC 码
│   └── (其余契约沿用 001，见下"契约同步"说明)
└── tasks.md             # Phase 2 输出（/speckit.tasks 生成）
```

**契约同步（与 001）**: `specs/001-…/contracts/subset-grammar.md` 的 EBNF 增加 `if-expression`
与 `if-equation` 产生式，并把 `if` 从"明确不支持"清单移出；`diagnostics-contract.md` 新增
MC 码追加于表尾（MC0204/0205/MC0305/0306）；`src/diagnostics/diagnostic.h` 同步新增枚举。

### Source Code (repository root)

```text
src/
├── lexer/                # "if/elseif/else/then/end" 已为关键字，无需词法改动
├── ast/                  # ast.h: Expr::If 节点；Equation 条件变体（分支列表）
├── parser/               # parsePrimary → if-expression；parseEquation → if-equation
├── semantic/             # symbols.cpp: 表达式以"布尔上下文"类型判断；
│                         #   equations.cpp: 分支平衡/同未知量校验 + 依赖并入图
├── codegen/              # plan.cpp: 条件方程→三元赋值归约；codegen.cpp: 三元发射
└── runtime/              # 无改动

tests/
├── unit/                 # test_parser / test_semantic / test_codegen(plan) 新增用例
├── golden/               # 新条件模型金样基线（如 case_21 见下）或并入 cooling 金样目录
└── integration/          # 端到端：转换→构建→运行→断言 CSV（条件切换数值断言）

examples/models/          # 新增 case_21_cond_expr.mo、case_22_cond_eq.mo、
                          #         case_23_cond_elseif.mo 等（SC-001 ≥5 个）
references/               # 如有 omc 环境则补参考 CSV（可选，容差用例可单测断言）
```

**Structure Decision**: 沿用 001 单一项目布局，功能增量全部落在既有 ast/parser/semantic/codegen/tests
上，不新增目录。条件方程在 semantic 阶段完成"分支=同一未知量的单方程"校验后，于 plan 阶段归约为
三元赋值，codegen 仅新增一种表达式发射（`If`），最大限度复用现有数据结构与生成骨架。

## Complexity Tracking

> Constitution Check 全部通过，无需要辩护的违规项。

## 附录: FR ↔ 实现/测试覆盖对照

| 需求 | 实现位置 | 测试证据 |
|------|----------|----------|
| FR-001 条件表达式（else 必选） | parser primary → ast::Expr::If；semantic 布尔条件/分支类型校验 | Parser.ConditionalExpression*；Semantic.IfConditionNotBoolean* |
| FR-002 条件方程 | parser equation → ast 条件方程变体；semantic 分支校验 | Parser.ConditionalEquation* |
| FR-003 elseif/嵌套 | parser 递归 BRANCHES；表达式内嵌套 if | Parser.ConditionalElseifChain*；NestedIf* |
| FR-004 非布尔条件拒绝 | semantic isBooleanExpr 校验 | Semantic.IfConditionNotBoolean 回归 |
| FR-005/006 诊断与确定性 | 新增 MC0204/0205/0305/0306；渲染沿用；金样比对 | Golden*；CliErrors-if-* |
| FR-007 标识符溯源 | AST 沿用 Token 定位；映射沿用 | 既有 PlanBuild.MapsIdentifiers 扩展 |
| FR-008 分支平衡 | semantic 校验各分支单方程+同未知量（缺 else→MC0305） | Semantic.IfBranchBalance* |
| FR-009 全构造测试 | unit/golden/integration 各层 | ctest 新增用例全绿 |
| FR-010 求值语义随文档 | contracts/if-construct-contract.md 发布步边界离散求值 | 集成用例：条件切换点断言 |
| SC-001 ≥5 模型闭环 | examples/models/case_21..26 | 集成/回归用例覆盖 |
| SC-003 非法输入拒绝 | MC0204/0205/0305/0306 + CliErrors | 三层非法样例 |
| SC-004 回归不回退 | 既有 53 用例 + 新增全部保持绿 | ctest 全量 |

（注：本方案把条件方程限定为"每分支恰一个方程、求解同一未知量"，是 Modelica 分支平衡
规则在单变量情形下的正规满足形式；多方程分支块视为计划外扩展，记录于 data-model.md。）