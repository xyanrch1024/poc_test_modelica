# 契约: if 条件表达式与条件方程

**Feature**: 003-if-statement-support | **Date**: 2026-09-12
依据 [spec.md](./spec.md) FR-001~FR-010 与 Clarifications 2026-09-12（Q1=A 两者都支持；Q2=A 运行期条件、步边界离散求值、无事件）。

## 1. 文法扩展（EBNF delta，追加至 001 subset-grammar.md）

```ebnf
equation        = if_equation | simple_eq ;

if_equation     = "if" , expression , "then" , { equation } ,
                  { "elseif" , expression , "then" , { equation } } ,
                  [ "else" , { equation } ] ,
                  "end" , "if" , ";" ;

primary         = NUMBER | BOOLEAN | IDENT
                | IDENT , "(" , [ args ] , ")"
                | "(" , expression , ")"
                | if_expression ;
if_expression   = "if" , expression , "then" , expression ,
                  "else" , expression ;
```

- `if`/`elseif`/`else`/`then`/`end` 已是词法关键字，无需词法改动。
- `if_equation` 分支体允许嵌套 `if_equation`（递归满足 equation 产生式）。
- `if_expression` 的 `else` **必选**，且允许嵌套（三元内再含 if_expression）。
- 既有 `simple_eq` 维持不变；`if_equation` 与表达式归约规则见 §3。

## 2. 语义规则

### 2.1 条件必须是布尔表达式（MC0204）
`if`/`elseif` 条件必须满足 `isBooleanExpr`（见 data-model.md）：布尔字面量、logical or/and/not、
关系比较、Boolean 标识符、布尔型嵌套 if 表达式。否则 [MC0204]。

### 2.2 分支平衡（MC0305，对应 MLS 8.3.4）
- 条件方程各分支的超集未知量**相同且只有一个**：每一分支**恰好一个方程**，且各分支 LHS
  为**同一未知量**（同一 `Ident` 或同一 `der(Ident)` 目标）。
- 缺省 `else` 计 0 个方程；因本契约分支方程数恒为 1，**带运行期条件的 if 方程必须显式 `else`**。
- 违反任意一条 → [MC0305]；缺 else 且条件为非常量（运行时不可静态选择）→ [MC0305]（语义错误）。
- **常量条件特例**：若所有条件为常量（`true`/`false` 构成、constant 字面量），缺省 `else`
  按 MLS 允许（该情形分支展开为静态选择），不报错——此即 MC0306 缺席条件。

### 2.3 分支类型一致性（MC0205）
- 条件表达式 then/else 值类型同类：数字（Real/Integer 隐式提升）↔ 数字，或布尔 ↔ 布尔；混合 → [MC0205]。
- `der(...)` 不得出现在分支方程 RHS 或条件内（MC0203 沿用）。

### 2.4 求值语义（Clarification Q2）
- 无事件检测/过零精化（既定子集边界，contract 状态明示属 POC 离散近似）。
- **具体求值时机**：条件表达式内联为 C++ 三元（§3），布尔在每次 RHS/`deriv` 求值点按当前
  状态值重采样——含 RK4 每个阶段点。即：切换在**首次使条件为真的求值点所在步内生效**
  （局部误差有界，量级 ≤ h/6，见 integration 用例容差）；CSV 行反映每一步结束时的分支取值。
- 无 else 且非常量条件在编译期拒绝（MC0305，见 §2.2），故不存在"维持上一步值"的运行期路径。
- 状态量分支：条件方程含 `der(x)` 分支时，各分支 LHS 必须同为 `der(x)`；生效分支决定该状态 `d.x`。

## 3. 代码生成约定
- `if_expression` → C++ 三元运算符 `(cond ? a : b)`；elseif 链 → 右结合嵌套三元。布尔结果显式 `true`/`false`。
- `if_equation`：先按 data-model.md §D4 归约为普通等式/状态 RHS 的三元表达式，再走既有通道；
  生成产物不出现语句级 `if/else` 块（保持直线赋值，001 的 RK4 骨架语义不变）。
- 确定性：条件标识符按 001 identifiers 映射；不嵌入时间戳/路径；浮点 `%.17g`（FR-007）。

## 4. 新增错误码（追加至 001 diagnostics-contract.md 表尾）

| 码 | 类别 | 触发条件 |
|----|------|----------|
| [MC0204] | 表达式 | if/elseif 条件非布尔表达式 |
| [MC0205] | 表达式 | if 表达式分支类型不一致（数字 vs 布尔） |
| [MC0305] | 方程 | 条件方程违反分支平衡（缺 else / 分支方程数≠1 / 分支 LHS 不同未知量） |
| [MC0306] | 方程 | 条件方程缺 else 且条件非常量（保留位：当前归并于 MC0305，本版本不触发） |

> MC0306 为语义预留码：缺 else 且非常量条件在完整 Modelica 下属非法（MLS 8.3.4 本意）。
> 本 POC 因分支单方程限制，任何缺 else 均已在 MC0305 拒绝；MC0306 编号保留供后续多方程
> 分支块扩展使用，现不触发。两条码同步登记到 001 diagnostics-contract.md 以保证编号唯一。

## 5. 契约同步要求（实现时落实）
- `specs/001-…/contracts/subset-grammar.md`：追加 §1 的 EBNF delta，将 `if` 从"明确不支持"清单移除。
- `specs/001-…/contracts/diagnostics-contract.md`：追加 §4 编码表。
- `src/diagnostics/diagnostic.h`：新增 `ExprIfConditionNotBoolean=204`、`ExprIfBranchTypeMismatch=205`、
  `EqIfBranchMismatch=305`、`EqIfMissingElse=306` 枚举（编号与数值按表尾续）。