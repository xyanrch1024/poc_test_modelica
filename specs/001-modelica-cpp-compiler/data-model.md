# Phase 1 数据模型: 编译器内部实体

**Feature**: 001-modelica-cpp-compiler | **Date**: 2026-08-24
配套契约见 [contracts/](./contracts/)；验证指南见 [quickstart.md](./quickstart.md)。

## 编译流水线与状态迁移

```text
SourceFile ──lex──▶ Token 流 ──parse──▶ AST ──analyze──▶ AnalyzedModel
                                                        │  (失败→Diagnostic 列表, exit 2)
AnalyzedModel ──sort/plan──▶ TranslationPlan ──generate──▶ GeneratedProject (目录)
```

每个阶段只消费上一阶段的不可变产物；任何阶段产生 error 级诊断即终止流水线，
不产出下游文件（FR-005、用户故事 3 场景 1）。

## 实体定义

### SourceFile（输入）
| 属性 | 说明 | 校验规则 |
|------|------|----------|
| path | 源文件路径 | 必须存在且可读，否则 exit 1 |
| text | UTF-8 文本 | 非空且含至少一个模型声明，否则 MC0001 |

### Token
| 属性 | 说明 |
|------|------|
| kind | 关键字/标识符/整数/实数/字符串/运算符/EOF |
| lexeme | 原文片段 |
| line, col | 1 起始行列号（诊断定位必需，FR-006） |

### AST（代数类型风格节点，含 SourceRange）
- `Model(name, components[], equations[], experiment?)` —— 一文件仅允许一个（FR-004）
- `Component(name, type ∈ {Real,Integer,Boolean}, kind ∈ {constant,param,var}, value?, start?, fixed?)`
- `EquationVariant`: `SimpleEquation(lhs, rhs)` | `DerEquation(target, expr)`
- `Expr`: `NumLit | Ident | Unary(op,e) | Binary(op,l,r) | Call(fn,args) | Der(e)`
  - `Call.fn` 仅允许白名单初等函数：abs/sqrt/sin/cos/tan/exp/log/log10/min/max
  - `Der(e)` 仅允许 `der(Ident)` 形式出现在方程左侧语境，否则 MC0203
- `Experiment(startTime, stopTime, interval)` —— 来自 `annotation(experiment(...))`

### SymbolTable（语义阶段构建）
| 规则 | 违规码 |
|------|--------|
| 标识符唯一性（重复声明） | MC0101 |
| 使用前必须声明 | MC0102 |
| constant 必须有初始化值 | MC0103 |
| parameter/var 的 start 缺省为 0.0 | —（默认规则） |

### EquationAnalysis（配平与排序结果）
| 规则 | 违规码 |
|------|--------|
| 方程数 == 未知量数（欠定/超定拒绝） | MC0301/MC0302 |
| 代数环检测（Tarjan SCC，size>1 或自环即拒绝） | MC0303 |
| der(x) 的 x 必须被 start 初始化或 fixed=true | MC0304 |
| 代数方程拓扑序存在（无环保证） → 求值顺序列表 | — |

### TranslationPlan（代码生成输入）
- states: 有序列表（按源文件声明序，保证确定性 FR-007）
- algebraic_steps: 拓扑排序后的赋值序列
- rhs_expr: 各状态导数表达式（已解析到 Ident 引用）
- experiment: 时间网格参数
- identifiers: 源名 → C++ 名映射（合法标识符直用；保留字加后缀 `_`，保持可溯源 FR-008）

### GeneratedProject（输出目录）
```text
<outdir>/
├── CMakeLists.txt          # 独立可构建（FR-002），固定最低 cmake 版本
├── model_<name>.cpp        # 模型专属代码（RK4 右端函数 + 主循环 + CSV 写出）
└── mcruntime/              # 从 src/runtime/ 复制的运行时头文件
    ├── rk4.hpp
    └── csv_writer.hpp
```
生成内容确定性：遍历全部使用有序结构、浮点以 `%.17g` 序列化、无时间戳/路径嵌入。

### Diagnostic
| 属性 | 说明 |
|------|------|
| severity | error / warning |
| location | file:line:col |
| code | MCxxxx 稳定编号（见 diagnostics-contract.md） |
| message | 人读原因描述 |

### ReferenceCase（回归资产，入库）
| 属性 | 说明 |
|------|------|
| model_file | examples/models/<name>.mo |
| reference_csv | references/<name>_res.csv（omc outputFormat="csv" 生成） |
| tolerance_rel / tolerance_abs | 默认 1e-4 / 1e-6，可在用例清单覆盖 |
| compare_columns | 参与比对的变量列集合 |

### ValidationReport（批量比对输出，stdout）
逐用例一行：`<case>: PASS|FAIL max_abs_dev=<n> at t=<t>`，末行汇总；
任一 FAIL 使进程退出非零（供 CI 直接判定）。
