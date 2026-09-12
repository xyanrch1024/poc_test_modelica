# Phase 1 数据模型: 条件表达式与条件方程内部实体

**Feature**: 003-if-statement-support | **Date**: 2026-09-12
配套契约见 [contracts/if-construct-contract.md](./contracts/if-construct-contract.md)；
验证指南见 [quickstart.md](./quickstart.md)。

## 编译流水线与状态迁移（增量）

```text
AST ──analyze──▶ AnalyzedModel ──sort/plan──▶ TranslationPlan ──generate──▶ GeneratedProject
                    │ 条件类型/分支校验（新增）
                    │ 分支并集依赖建边（新增）
```

流水线仍为单程、不可逆；任何阶段产生 error 即终止（FR-005）。条件构造不引入新的阶段。

## 实体定义（增量）

### Expr（新增变体）
| 属性 | 说明 | 校验规则 |
|------|------|----------|
| `Expr::If(cond, thenExpr, elseExpr)` | 三元条件表达式 | 条件须 `isBooleanExpr(cond)`（否则 MC0204）；then/else 类型须同类（数字↔数字或布尔↔布尔，否则 MC0205）；else 必选 |

`isBooleanExpr(e)` 判定闭包：
- `BoolLit` (true/false)；`Unary(:not)`、`Binary(:and/:or)`
- `Binary` 关系运算符（`< == > <= >= <>`）
- `Ident` 且 SymbolTable 中类型为 Boolean
- `Expr::If` 且 then/else 均为布尔
- 其余（含 `Der`、函数调用、算术）→ false

### Equation（新增条件变体）
| 属性 | 说明 | 校验规则 |
|------|------|----------|
| `IfEquation(branches)` | 分支列表 `(cond?, equations)`，末条可为无条件的 else | 每分支**恰好一个方程**；各分支 LHS 必须为**同一未知量**（同一 Ident 或同一 `der(Ident)` 目标），违反则 MC0305 |
| 分支方程 | 普通 `simple_eq`（`lhs = expr`）或 `der_eq`（`der(x) = expr`） | 复用 001 的派生/普通等式校验 |

条件方程的分支条件（非 else 分支）同样须 `isBooleanExpr` → 否则 MC0204。
`der(...)` 仅允许出现在分支等式 LHS 顶层（MC0203 沿用）。

### 分支平衡规则（对应 plan D1）
- 各非 else 分支 1 个方程，缺省 else 计 0 → 因此带运行期条件的 if 方程**必须显式 else**（MLS 8.3.4）；
  缺 else 时若条件为纯常量，可等价展开（见 contract §3）；两情形均满足"方程数与未知量数恒定"。
- 违反（缺 else + 非常量条件 / 分支方程数≠1 / 分支 LHS 不同未知量）→ MC0305。

### EquationAnalysis（配置平校验增量）
- 条件方程的未知量 = 分支 LHS 的单一未知量（并入总体未知量计数，MC0301/0302 逻辑不变）。
- 代数依赖图：节点 = 未知量；对条件方程 `v = f(c, e1,…,en)` 建边
  `v ← vars(cond) ∪ vars(e1) ∪ … ∪ vars(en)`（**分支并集**，plan D2）。
- 环检测沿用 Tarjan SCC（MC0303）；仅为保守并集的伪环属于已知边界（research §5）。
- 状态量条件分支：`der(x)` 若出现在任一分支 LHS，则 **所有**分支 LHS 均须为 `der(x)`（同一状态）；
  满足后该状态 RHS 为三元表达式 `d.x = c ? e1 : … : en`。

### TranslationPlan（新增字段）
| 字段 | 说明 |
|------|------|
| `if_exprs` | plan 阶段由条件方程归约出的条件赋值（见下） |
| 求值顺序 | 条件方程与其 LHS 未知量参与既有拓扑排序，排序后以三元赋值形式进入 steps |

### 条件方程 → 三元赋值归约（plan D4，semantic→plan 边界）
- `if c then v = e1; elseif c2 then v = e2; else v = e3; end if;`
  → 普通等式 `v = e1` 的 RHS 替换为 `If(c, e1, If(c2, e2, e3))`；
- `if c then der(x) = e1; else der(x) = e2; end if;`
  → 状态 RHS 字典项 `x → If(c, e1, e2)`；
- 归约在 `TranslationPlan` 构建层完成，**codegen 只看见 Expr::If 与普通等式**，
  生成产物保持直线赋值结构（无语句级 if/else）。

### Expr::If 的代码生成（codegen 增量）
- 数值结果 → 生成 `(/*cond*/ ? e1 : e2)`（右结合嵌套表达 elseif 链）；
- 布尔结果 → 生成 `(cond ? true : false)`；
- 条件中的标识符映射、`%.17g` 序列化均沿用 001 规则，确定性不变（FR-007）。

## 状态/常量/参数与运行期条件
- 条件引用 constant/parameter → 编译期可求值，仍按表达式发射（不特判）；语义一致（常数条件总是同一分支）。
- 条件引用状态量/代数量 → 每次输出步按当前值求值（步边界离散，Clarify Q2），生成代码无需特殊结构。

## 未知量守恒证明（单赋值规则）
- 每条件方程贡献恰 1 个未知量（分支 LHS 唯一未知量）与 1 个方程；else 必选保证任何时刻恰 1 分支激活、恰 1 方程生效 → 方程数恒定。
- 各分支等式在语义上为同一未知量的不同 RHS，expand 后不存在"某时刻缺方程"。

## 新增诊断码（与 contracts 同步）
| 码 | 类别 | 触发条件 |
|----|------|----------|
| MC0204 | 表达式 | if/elseif 条件非布尔表达式 |
| MC0205 | 表达式 | if 表达式分支类型不一致（数字 vs 布尔） |
| MC0305 | 方程 | 条件方程违反分支平衡（缺 else / 分支方程数≠1 / 分支 LHS 不同未知量） |
| MC0306 | 方程 | 条件方程缺 else 且条件非常量（不可静态选定分支） |

> MC0305 与 MC0306 为依次判定：先平衡（MC0305），再常量性（MC0306）。缺 else 时恒报 MC0305；常量条件下缺 else 可化归为分支整体展开而放行。