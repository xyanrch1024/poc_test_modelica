---

description: "Task list for 003: if conditional expression & conditional equation support"
---

# Tasks: 支持 if 语句（条件表达式与条件方程）

**Input**: Design documents from `/specs/003-if-statement-support/`

**Prerequisites**: plan.md, spec.md (user stories), research.md (D1–D5), data-model.md, contracts/if-construct-contract.md

**Tests**: REQUIRED. spec.md §User Scenarios & Testing 为强制章节，且 FR-009 明确要求"每个受支持 if 构造至少
一个自动化测试覆盖其转换→运行行为；每个已修复缺陷附带回归测试"。因此每个用户故事均含测试任务（TDD：先写用例，红→实现→绿）。

**Organization**: 任务按用户故事组织，支持独立实现/独立测试。

## Format

- **TaskID**: T001… 按执行顺序
- **[P]**: 可与同阶段其他 [P] 任务并行（不同文件、无依赖）
- **[Story]**: US1/US2/US3（对应 spec.md 三个用户故事）
- **Checklist**: 一律 `- [ ] <ID> [P] [Story] 描述（含精确文件路径）`

## 关键设计约定（必读）

- **分支平衡（MLS 8.3.4 → plan D1）**：条件方程每分支**恰一个方程**且各分支 LHS 为**同一未知量**；
  带运行期条件时**必须显式 else**；违反 → MC0305。
- **归约（plan D4 / data-model）**：条件方程在 `analyzeEquations` 归约为三元 IfExpr 赋值
  （代数 `v = If(c,e1,e2)`、状态 `d.x = If(c,e1,e2)`），codegen 只把 IfExpr 发射为 C++ 三元 `(c ? a : b)`，
  **不产生语句级 if/else 块**——生成产物保持直线赋值，与 001 基线同构。
- **依赖并集（plan D2）**：代数/状态依赖边 = `vars(cond) ∪ vars(e1) ∪ … ∪ vars(en)`。
- **求值语义（Clarify Q2 / FR-010）**：每次输出步按当前值布尔求值，切换在步边界生效，无事件精化。
- 新 MC 码：MC0204（条件非布尔）、MC0205（分支类型不一致）、MC0305（分支平衡违反）、MC0306（预留不触发）。

---

## Phase 1: Setup（共享基础设施）

**Purpose**: 契约与错误码底座先行，保证下游文件以正确编号实现

- [X] T001 [P] 在 `specs/001-modelica-cpp-compiler/contracts/subset-grammar.md` 追加
  if_equation 与 if_expression 的 EBNF（按 003 contracts/if-construct-contract.md §1），
  并从"明确不支持"清单移除 `if`
- [X] T002 [P] 在 `specs/001-modelica-cpp-compiler/contracts/diagnostics-contract.md` 表尾追加
  MC0204/0205/MC0305/MC0306 四行（按 003 contracts §4，编号不回退）
- [X] T003 [P] 在 `src/diagnostics/diagnostic.h` 的 `Code` 枚举末尾新增
  `ExprIfConditionNotBoolean = 204`、`ExprIfBranchTypeMismatch = 205`、
  `EqIfBranchMismatch = 305`、`EqIfMissingElse = 306`，注释指向 003 契约

---

## Phase 2: Foundational（阻塞性前置）

**⚠️ CRITICAL**: 此阶段完成前不得开始任何用户故事。AST 与布尔判定是所有 story 的公共地基。

- [X] T004 在 `src/ast/ast.h` 扩展表达式与方程表示：
  - `ExprKind` 新增 `If`；`Expr` 新增 `cond/thenExpr/elseExpr` 三个 `ExprPtr` 成员
    与工厂 `static ExprPtr if_(Token tok, ExprPtr cond, ExprPtr thenExpr, ExprPtr elseExpr)`
  - `struct Equation` 新增可选条件变体（如 `std::optional<IfEquation> ifEq`）；
    `struct IfEquation { struct Branch { ExprPtr condition; /* else 分支为 nullopt */ std::vector<Equation> equations; }; std::vector<Branch> branches; }`
  - 所有既有工厂/字段保持不动，避免破坏现有构造点
- [X] T005 在 `src/semantic/symbols.h` 声明、`src/semantic/symbols.cpp` 实现
  `bool isBooleanExpr(const ast::Expr &, const SymbolTable &)`：按 data-model.md §isBooleanExpr 闭包规则
  （BoolLit / not / and / or / 关系比较 / Boolean 标识符 / 布尔 if 表达式），依赖 T004 的 `ExprKind::If`

**Checkpoint**: 编译通过（`cmake --build build`），既有 53 个 ctest 保持全绿（纯增量，不含语义行为改动）。

---

## Phase 3: User Story 1 - 条件表达式 & 条件方程基础 (Priority: P1) 🎯 MVP

**Goal**: 建模工程师可用二分支条件表达式 `y = if c then a else b` 与条件方程
`if c then v = e1; else v = e2; end if`（含 `der(x)` 分支）表达分段行为，走通
转换→构建→运行→验证 CSV，且两次运行逐字节一致。

**Independent Test**: 给定行为已知的分段模型（如 case_21），独立执行转换→构建→运行→检查轨迹四步，
全程无人工修改生成物；触发与不触发条件的区间分别符合分支语义；同一产物运行两次输出 CSV 一致。

### Tests for User Story 1（先写，红→绿）

- [X] T006 [P] [US1] 在 `tests/unit/test_parser.cpp` 新增条件构造解析用例：
  表达式 `y = if x > 0 then a else b`、条件方程
  `if b then v = 1; else v = 2; end if`、`der(x)` 分支形态
  （断言 AST：ExprKind::If 的三子节点、IfEquation 的分支结构与 Token 定位）
- [X] T007 [P] [US1] 在 `tests/unit/test_semantic.cpp` 新增语义用例：
  isBooleanExpr 闭包全覆盖（关系/逻辑/布尔变量/嵌套）、合法条件方程分析产出
  `algebraicSteps`/`stateDefs` 中携带三元 IfExpr RHS、非布尔条件→MC0204（含 file:line:col）、
  分支类型不一致→MC0205、缺 else/分支方程数≠1/分支不同未知量→MC0305

### Implementation for User Story 1

- [X] T008 [US1] 在 `src/parser/parser.cpp` 实现 `parseIfExpression`：
  从 `kUnsupportedKeywords()`（parser.cpp:12）移除 `"if"`；在 `parsePrimary()`（parser.cpp:525）遇到
  `if` 关键字时分派到 parseIfExpression，解析 `if <expression> then <expression> else <expression>`，
  `else` 缺省时报带定位的错误（进入 US3 诊断通路前先返回 nullopt + synchronizeStatement）
- [X] T009 [US1] 在 `src/parser/parser.cpp` 实现 `parseIfEquation`（二分支）：
  在 `parseEquation()`（parser.cpp:348）入口分派——`if` 为关键字，若 peek 即 `if` 则走条件方程路径，
  否则维持原有 simple_eq 路径；解析 `if <expr> then <branch_eq>; else <branch_eq>; end if;`，
  分支体递归调 `parseEquation`（方程序号与既有一致），以 `elseif/else/end` 关键字结束分支收集；
  2 分支暂不涉及 elseif（US2 扩展）
- [X] T010 [US1] 在 `src/semantic/symbols.cpp` 扩展 `analyzeExpressions`：
  遍历时递归进入 if 表达式三子节点与 if 方程各分支方程体（不遗漏 MC0102/MC0202/MC0203 检查）；
  条件位置调 isBooleanExpr，不满足→MC0204；if 表达式 then/else 分支若不满足"全布尔或全数字"→MC0205；
  若条件内出现 `Der` 节点→MC0203（der 仅可出现在方程左侧）
- [X] T011 [US1] 在 `src/semantic/equations.cpp` 实现条件方程分析：
  分支平衡校验（各分支 equation==1、各分支 LHS 同未知量；缺 else 且非常量→MC0305）；
  归约：代数 LHS 未知量 `v` → `algebraicSteps.push_back({v, Expr::if_(c, e1, e2)})`，
  状态 LHS `der(x)` 各分支一致时 → `stateDefs.push_back({x, IfExpr})`；
  依赖建边（plan D2）：RHS 依赖集 = vars(cond) ∪ vars(e1/e2…)，并入既有拓扑排序与 Tarjan 环检测
  （MC0301/0302/0303 复用，不报新码）
- [X] T012 [US1] 在 `src/codegen/codegen.cpp` 表达式发射新增 `ExprKind::If` 分支：
  生成 `( /*cond*/ ? then : else )` 三元，递归发射子表达式；布尔结果显式 `true`/`false`；
  浮点/标识符映射规则沿用（`%.17g`、mapToCppIdentifier），保证逐字节确定性
- [X] T013 [P] [US1] 新增 `examples/models/case_21_cond_expr.mo`：
  含状态量 + 运行期条件表达式 `y = if x > 1 then y0 + slope*(x-1) else y0 + x`、
  `der(x) = 1`、experiment 注释（quickstart.md 场景 A）
- [X] T014 [P] [US1] 新增 `examples/models/case_22_cond_const.mo`：
  纯 constant/parameter 条件的 if 表达式与嵌套 if 表达式（quickstart.md 场景 B 锚点）
- [X] T015 [P] [US1] 新增 `examples/models/case_23_cond_eq.mo`：
  条件方程（elseif 链的 case 见 US2）+ `der(s)` 分支与 if 表达式混用（quickstart.md 场景 C 语义自校验点）
- [X] T016 [US1] 新增 `tests/integration/test_conditional.cpp`（并注册进 tests/CMakeLists.txt，见 T018）：
  case_21/22/23 逐一 转换（modelicac translate）→构建（cmake -S/-B）→运行（生成二进制）→ 读取 CSV；
  断言：分支区间数值、t≈切换步边界的值、**同程序运行两次 CSV 逐字节一致**（FR-006），并在 tests/CMakeLists.txt 登记
- [X] T017 [US1] 新增金样 `tests/golden/test_golden_conditional.cpp` + `tests/golden/conditional/` 基线目录：
  用真实输出生成 `model_Case21CondExpr.cpp`、`CMakeLists.txt`、`mcruntime/{rk4,csv_writer}.hpp` 作为入库基线，
  测试断言 modelicac 输出与基线逐字节一致（锁定确定性，FR-007）
- [X] T018 [US1] 在 `tests/CMakeLists.txt` 统一登记新增目标（test_conditional、test_golden_conditional）
  及新增金样路径变量；确认 ctest 发现新用例

**Checkpoint**: `ctest --test-dir build` 中既有 53 + 新增全部通过；MVP 可独立交付。

---

## Phase 4: User Story 2 - elseif 多分支与嵌套 (Priority: P2)

**Goal**: 支持 `elseif` 链与嵌套 `if`（表达式内嵌 if、条件方程内嵌套），多分支按声明顺序求值，产出确定。

**Independent Test**: 提交含 elseif 与嵌套 if 的样例（case_24/25），转换→运行后各段分支行为与声明顺序一致，输出确定。

### Tests for User Story 2

- [X] T019 [US2] 在 `tests/unit/test_parser.cpp` 新增用例：三/四段 elseif 链解析
  （断言 branches 顺序与各分支 condition 定位）、表达式内嵌套 if、条件方程内嵌套 if 方程
- [X] T020 [US2] 在 `tests/unit/test_semantic.cpp` 新增用例：elseif 链条件各段均校验 MC0204、
  嵌套归约为右结合嵌套三元并进入拓扑排序、多分支并集依赖不误报代数环的正例（research §5）

### Implementation for User Story 2

- [X] T021 [US2] 在 `src/parser/parser.cpp` 扩展 parseIfEquation：循环消费 `elseif <expr> then <eq>;`，
  保留可选 `else`；分支体递归 `parseEquation`（自然支持嵌套条件方程）；保证与 T008 的表达式嵌套互通
- [X] T022 [US2] 在 `src/semantic/equations.cpp` + `src/codegen/codegen.cpp` 验证/加固多分支路径：
  分支平衡判定推广到 n 分支（各分支仍恰 1 方程、LHS 同未知量）；归约生成右结合嵌套 IfExpr；
  codegen 递归发射嵌套三元（C++ 右结合天然匹配）；依赖并集按各分支取并
- [X] T023 [P] [US2] 新增 `examples/models/case_24_cond_elseif.mo`：
  三段分段控制模型（如快/正常/慢挡位，对应 3 分支 elseif 链 + 状态导数切换）
- [X] T024 [P] [US2] 新增 `examples/models/case_25_cond_nested.mo`：
  嵌套 if 表达式（三元内嵌三元）与嵌套条件方程的正例
- [X] T025 [US2] 扩展 `tests/integration/test_conditional.cpp`：追加 case_24/25 闭环断言
  （各分段数值、elseif 顺序求值、嵌套归约结果、双运行确定性），并同步金样基线目录（如需）

**Checkpoint**: US1 + US2 用例全部绿；既有用例不回退。

---

## Phase 5: User Story 3 - 非法 if 构造的清晰诊断 (Priority: P3)

**Goal**: 不完整/非法 if（缺 then/else/end if、条件非布尔、der 在条件内、分支不平衡）被拒绝，
诊断含 file:line:col 与原因，exit 2，不产出生成物；多错误尽量一次报全（FR-005）。

**Independent Test**: 逐条提交非法 if 样例，核对诊断含文件名、行号、原因；失败时不产生输出目录。

### Tests for User Story 3

- [X] T026 [P] [US3] 在 `tests/unit/test_parser.cpp` 新增负例：
  缺 `then`、缺 `else`、缺 `end if`、`if` 后非表达式——断言报错、Token 定位、并吞掉异常 token 后继续解析（同步恢复）
- [X] T027 [P] [US3] 在 `tests/unit/test_diagnostics.cpp` 新增用例：
  含多个 if 错误的输入（如同文件 3 处非法）经 DiagnosticCollector 一次输出全部（按 (file,line,col) 升序），验证 FR-005"尽可能一次报全"
- [X] T028 [US3] 在 `tests/integration/test_cli_errors.cpp` 新增 if 非法输入用例：
  缺 else 表达式（→解析错误，位置在 else 预期处）、条件非布尔（→MC0204）、分支 LHS 不同未知量（→MC0305）、
  `der(x)` 出现在条件内（→MC0203）；断言 stderr 文本格式 `[error] file:line:col: … [MCxxxx]`、进程退出码 2、无输出目录

### Implementation for User Story 3

- [X] T029 [US3] 在 `src/parser/parser.cpp` 完善 parseIfExpression/parseIfEquation 错误路径：
  缺 `then/else/end if` 各给带定位的明确消息（复用 `expect` 失败码或新增针对性文本）、`synchronizeStatement` 恢复、
  不因单点错误中断整文件分析（支撑一次报全）
- [X] T030 [US3] 在 `src/semantic/symbols.cpp` 与 `src/semantic/equations.cpp` 补齐定位化诊断：
  条件非布尔→MC0204 指向条件首 token；分支类型不一致→MC0205 指向 then 分支；分支不平衡→MC0305 指向 if 关键字；
  条件内 der→MC0203 指向 der token；确认 MC0306 不在任何路径触发（预留位，契约 §4）

**Checkpoint**: US3 诊断用例全绿；`MC0306` 不触发且被单元测试显式断言。

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 边缘用例、性能护栏、文档与全量质量门禁（SC-001~004）

- [X] T031 [P] 常量条件静态分支选择（Edge Cases"常量真/假折叠"）：在 `src/semantic/equations.cpp`
  对条件可求值（evalConstExpr 返回 true/false）的 if 方程直接选择活跃分支（缺 else 仅当条件为常量时允许，
  即 MC0306 具象化）；配套 `tests/unit/test_semantic.cpp` 用例
- [X] T032 [P] 嵌套深度护栏（Edge Cases"嵌套过深"）：在 `src/parser/parser.cpp` 加显式嵌套深度计数，
  超限（如 10_000）报清晰失败而非栈溢出；配套 `tests/unit/test_parser.cpp` 用例
- [X] T033 [P] 新增 `examples/models/case_26_cond_constif.mo`（常量条件 if 方程 + 缺 else），
  并在 `tests/integration/test_conditional.cpp` 追加该场景闭环断言（常量折叠与运行期结果一致）
- [X] T034 在 `tests/integration/test_performance.cpp` 追加深度嵌套 if 模型的性能护栏：
  深层嵌套（如 500 层三元）转换耗时低于阈值且不崩溃；既有 <10s 端到端上限不回退
- [X] T035 [P] 文档更新（FR-010 求值语义 MUST 随文档发布）：在 `README.md` 与 `example/README.md`
  增补受支持 if 构造清单、MC 码表、步边界离散求值语义说明
- [X] T036 运行 `specs/003-if-statement-support/quickstart.md` 场景 A–E 全量验证
  （含场景 D 诊断路径、场景 E 全测试绿）；如设计文档与实现出现漂移，回更 plan/data-model/contracts
- [X] T037 全量质量门禁：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
  → `ctest --test-dir build --output-on-failure` 全绿（既有 53 + 新增零回退 = SC-004）；
  若 references 存在则运行 `python3 tools/regression/run_regression.py examples/models/references.cases` 确认回归；
  提交前跑 clang-format/clang-tidy 门禁（注意：系统 clang-format 18.1.3 与项目锁定 17.x 的已知基线差异，
  若为环境差异以项目锁定版本为准）

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup（P1）**: 无依赖，立即开始；T001/T002/T003 相互独立可并行
- **Foundational（P2）**: 依赖 Setup；T004（AST）→ T005（isBooleanExpr）
- **US1（P3）**: 依赖 T004/T005；T008/09（parser）→ T010/11（semantic）→ T012（codegen）→ T016/17（逼近真实链路）
- **US2（P4）**: 依赖 US1；仅扩展 parser/semantic/codegen 的多分支路径
- **US3（P5）**: 依赖 US1（诊断建立在已实现的构造之上）
- **Polish（P6）**: 依赖前述全部

### 用户故事间依赖

- **US1（P1）**: 无故事依赖，MVP
- **US2（P2）**: 依赖 US1 的分支基础（迭代增强）
- **US3（P某）**: 逻辑上依赖 US1 的构造实现（诊断对象两分支路径）；可并行处理 PS3 的单元负例但集成断言在 US1 之后

### Story 内部顺序

- 测试先行（T006/07、T019/20、T026/27）→ 实现（parser→semantic→codegen）→ 模型/金样/集成 → Checkpoint

### Parallel Opportunities（跨 Story 现实约束）

编译器仓库多任务共享 parser.cpp/symbols.cpp/equations.cpp/codegen.cpp/test_parser.cpp/test_semantic.cpp，
**同文件任务必须串行**；真正的并行窗口在：
- Setup 三任务（T001/02/03）互不冲突
- 各 Story 的 examples/models/*.mo 新增彼此独立（T013/14/15、T023/24、T033）可并行
- 各 Story 的测试任务若落在**不同**测试文件（T016 与 T028、T006 与 T019/20 同文件则不可并）可并行
- Polish 的 T031/32/33/35 相互独立可并行

---

## Parallel Example: 单开发串行关键路径（推荐）

```bash
# Phase 3 最短串行链（单人 POC 建议顺序）：
Task: "T008 parseIfExpression in src/parser/parser.cpp"
Task: "T009 parseIfEquation in src/parser/parser.cpp"
Task: "T010 symbols.cpp 条件校验"
Task: "T011 equations.cpp 分支平衡+依赖并集+归约"
Task: "T012 codegen.cpp 三元发射"
Task: "T016 tests/integration/test_conditional.cpp"
Task: "T037 全套质量门禁"
```

## Parallel Example: 多开发并行

```bash
# 同时推进（文件互不冲突）：
Task: "T013 case_21_cond_expr.mo"          # examples/models/
Task: "T014 case_22_cond_const.mo"          # examples/models/
Task: "T015 case_23_cond_eq.mo"             # examples/models/
Task: "T031 常量条件静态分支选择"              # src/semantic/equations.cpp + 单测
Task: "T032 嵌套深度护栏"                     # src/parser/parser.cpp + 单测
Task: "T035 README 文档"                     # README.md / example/README.md
```

---

## Implementation Strategy

### MVP First（仅 US1）

1. Phase 1 Setup（三契约任务）
2. Phase 2 Foundational（AST + isBooleanExpr）
3. Phase 3 US1（两分支条件表达式 + 条件方程 + der 分支 + 集成/金样）
4. **STOP and VALIDATE**：case_21/22/23 闭环 + ctest 全绿 → MVP 可交付
5. 之后再进入 US2/US3/Polish

### Incremental Delivery

1. Setup + Foundational → 地基就绪
2. US1 → 独立测试 → MVP
3. US2（elseif/嵌套）→ 独立测试
4. US3（诊断）→ 独立测试
5. Polish（常量折叠/深度护栏/性能/文档/回归门禁）

### 关键增量不变量（每一阶段都必须保持）

- 生成产物保持**直线赋值、无语句级 if/else 块**（D4 归约铁律，金样逐字节锁定）
- 既有 20+ 模型与 53 测试用例全程不回退（SC-004）
- 任何新 MC 编码只追加、不改编号（contracts 表尾追加规则）

---

## Notes

- 每个任务完成后立即提交（commit after each task / logical group），消息风格见 `git log`
- 若解析层需同步 `src/parser/parser.h` 新私有方法声明，随对应任务一并修改
- 条件方程归入既有 equation 迭代（parser.cpp:179 调用点），确保不破坏 `annotation` 段的 `if` 判断
- 验证方式：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j` 后
  `ctest --test-dir build --output-on-failure`
- 所有新模型遵循 `case_<nn>_<snake>.mo` 命名与专家审阅的解析对照惯例；金样基线用真实输出生成后入库