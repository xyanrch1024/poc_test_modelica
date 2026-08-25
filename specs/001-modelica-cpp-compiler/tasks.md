# Tasks: Modelica 到 C++ 的编译器系统

**Input**: Design documents from `/specs/001-modelica-cpp-compiler/`

**Prerequisites**: plan.md ✅ | spec.md ✅ | research.md ✅ | data-model.md ✅ | contracts/ ✅

**Tests**: 强制包含——项目宪法原则 II 要求所有功能必须有自动化测试（FR-009），
每个故事阶段先写失败测试再实现。

**Organization**: 按用户故事分组（US1=P1 核心翻译、US2=P2 回归验证、US3=P3 诊断）。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成依赖）
- **[Story]**: 所属用户故事（仅故事阶段标注）
- 所有路径相对仓库根目录

## Path Conventions

单一项目布局（plan.md 已定）：`src/`、`tests/`、`tools/regression/`、
`examples/models/`、`references/`。

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 工程骨架与工具链固定（research.md R9 版本基线）

- [X] T001 创建根 CMake 工程：CMakeLists.txt（C++20、GoogleTest v1.14.0 FetchContent、enable_testing）并按 plan.md 结构建目录 src/{cli,lexer,ast,parser,semantic,codegen,diagnostics,runtime}、tools/regression、tests/{unit,golden,integration}、examples/models、references/
- [X] T002 [P] 配置代码风格门禁：仓库根添加 .clang-format（基于 LLVM，列宽 100）
- [X] T003 [P] 固定回归工具依赖：创建 tools/regression/requirements.txt（pytest>=8）
- [X] T004 编写 README.md：工具链版本表（GCC≥12/CMake≥3.24/GTest v1.14.0/OpenModelica≥1.23）+ 构建/测试命令

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 全部用户故事的阻塞前置——诊断基建、词法、AST

**⚠️ CRITICAL**: 本阶段完成前不得开始任何用户故事

### Tests for Foundational ⚠️（先写，确认失败）

- [X] T005 [P] 诊断模块单元测试：格式渲染 `[error] f.mo:1:2: msg [MCxxxx]`、(line,col) 排序、error 存在判定，在 tests/unit/test_diagnostics.cpp
- [X] T006 [P] 词法单元测试：子集全部 token 类别、字符串/实数/注释处理、行列号追踪准确性，在 tests/unit/test_lexer.cpp

### Implementation

- [X] T007 实现诊断模型与 MC 错误码注册表（contracts/diagnostics-contract.md 全表）：src/diagnostics/diagnostic.h 与 diagnostic.cpp
- [X] T008 [P] 实现 SourceRange/Token 类型与递归词法分析器（contracts/subset-grammar.md 的词法面），含保留字表：src/lexer/lexer.h 与 lexer.cpp
- [X] T009 [P] 定义 AST 节点（data-model.md：Model/Component/EquationVariant/Expr/Der/Experiment，均携带 SourceRange）：src/ast/ast.h

**Checkpoint**: 基础设施就绪，可开始用户故事实现

---

## Phase 3: User Story 1 - 将 Modelica 模型转换为可运行的 C++ 程序 (Priority: P1) 🎯 MVP

**Goal**: 合法子集模型 → 零修改可构建运行的 C++ 工程 → CSV 轨迹（spec 用户故事 1）

**Independent Test**: 对 examples/models/cooling.mo 执行 转换→构建→运行，
核对 CSV 表头 `time,T`、501 行、t=5 时 T≈20.66（quickstart.md 场景 A）

### Tests for User Story 1 ⚠️（先写，确认失败）

- [X] T010 [P] [US1] 解析器单元测试：覆盖 subset-grammar.md 每条产生式（模型/组件/属性/方程/表达式各优先级/实验注释/非法样例），在 tests/unit/test_parser.cpp
- [X] T011 [P] [US1] 语义单元测试：MC0101 重复声明、MC0102 未声明、MC0103 constant 缺初值、MC0202 白名单外函数、MC0203 der 位置、MC0301/MC0302 欠定超定、MC0303 代数环、MC0304 状态缺初值、MC0401 实验缺失，在 tests/unit/test_semantic.cpp
- [X] T012 [P] [US1] 金样测试基架：cooling.mo 代码生成输出与 tests/golden/cooling/ 基线逐字节比对（断言 FR-007 确定性），在 tests/golden/test_golden_cooling.cpp
- [X] T013 [P] [US1] 运行时单元测试：RK4 对解析解 dx/dt=-k(x-Tamb) 的收敛与逐步确定性、CSV 写出 %.17g 格式与 C locale，在 tests/unit/test_runtime.cpp

### Implementation

- [X] T014 [US1] 实现递归下降解析器（完整文法 + 实验注释解析，错误经诊断收集器上报）：src/parser/parser.h 与 parser.cpp（依赖 T008/T009）
- [X] T015 [US1] 实现符号表构建与声明检查（唯一性/使用前声明/constant 初值/默认 start=0）：src/semantic/symbols.h 与 symbols.cpp
- [X] T016 [US1] 实现方程分析：方程/未知量配平、der() 状态标记、Tarjan SCC 代数环检测、代数方程拓扑排序：src/semantic/equations.h 与 equations.cpp
- [X] T017 [US1] 实现 TranslationPlan 构建：有序状态列表、拓扑赋值序列、导数表达式、时间网格、标识符映射（保留字加 `_` 后缀并旁注原名，FR-008）：src/codegen/plan.h 与 plan.cpp
- [X] T018 [US1] 实现运行时头文件库：经典固定步长 RK4（发散检测钩子）+ CSV 写出器：src/runtime/rk4.hpp 与 src/runtime/csv_writer.hpp
- [X] T019 [US1] 实现代码生成器：产出独立 CMakeLists.txt 与 model_<name>.cpp（零参数 main、RK4 主循环、每网格点写行、无时间戳，contracts/generated-program-contract.md）：src/codegen/codegen.h 与 codegen.cpp
- [X] T020 [US1] 实现 CLI 入口 `modelicac translate <in> [-o dir]`：退出码 0/1/2、失败清理半成品输出目录（contracts/cli-contract.md）：src/cli/main.cpp（依赖 T007/T014–T019）
- [X] T021 [US1] 端到端集成测试：CTest 驱动 cooling.mo 转换→CMake 构建→运行→CSV 断言（行数/表头/终值容差），即 quickstart 场景 A 自动化，在 tests/integration/test_end_to_end.cpp
- [X] T022 [US1] 参数化验证示例与测试：examples/models/cooling_param.mo 改变 k 后轨迹随之变化（spec 用户故事 1 场景 3），接入 tests/integration/test_end_to_end.cpp

**Checkpoint**: User Story 1 可独立演示（MVP 达成）

---

## Phase 4: User Story 2 - 翻译正确性的批量回归验证 (Priority: P2)

**Goal**: 批量 转换→运行→与 OpenModelica 参考 CSV 比对，量化偏差与通过结论（spec 用户故事 2）

**Independent Test**: 对含基线的用例集执行 run_regression.py 得逐用例 PASS/FAIL；
人为注入翻译缺陷后受影响用例转 FAIL

### Tests for User Story 2 ⚠️（先写，确认失败）

- [X] T023 [P] [US2] 比对器 pytest 测试：混合容差判定 |a-b| ≤ tol_abs + tol_rel*|b|、网格插值取样、最大偏差定位、FAIL 退出码，在 tools/regression/test_compare.py

### Implementation

- [X] T024 [US2] 实现比对器（contracts/csv-result-schema.md 比对规则：基准网格线性插值、逐变量逐点偏差、ValidationReport 行格式）：tools/regression/compare.py
- [X] T025 [P] [US2] omc 参考生成脚本（simulate(..., outputFormat="csv") 调用模板）与裁剪脚本（裁剪为标准 schema：time+声明变量列）：references/gen_reference.mos 与 tools/regression/fetch_reference.py；记录版本于 references/VERSIONS.md
- [X] T026 [US2] 用例清单格式与批量驱动器：examples/models/references.cases（用例名|容差覆盖）+ tools/regression/run_regression.py（串起转换→构建→运行→比对→`SUMMARY: N/N PASS`，任一 FAIL 退出非零）
- [X] T027 [P] [US2] 代表性模型集第一批（10 个：常量/参数/各算术运算/关系逻辑/初等函数/多状态/纯代数方程/初始条件变体）：examples/models/case_*.mo
- [ ] T028 [US2] 代表性模型集第二批（10 个组合场景）并用 fetch_reference.py 生成入库 references/*_res.csv（达成 SC-001 的 ≥20 用例）
      ⚠️ 部分完成：20 个模型已入库并全部通过转换验证；参考 CSV 基线待装有 OpenModelica 的环境执行 fetch_reference.py 后补齐（见 references/VERSIONS.md）。
- [X] T029 [US2] 回归套件接入 CTest（omc 不可用时自动跳过并提示），在 tests/integration/CMakeLists.txt
      （模型集部分已完成；基线生成是唯一待办）

**Checkpoint**: 用户故事 1 与 2 均独立可用；SC-001/SC-002 可度量

---

## Phase 5: User Story 3 - 非法或不支持输入的清晰诊断 (Priority: P3)

**Goal**: 全部失败情形给出 file:line+原因诊断，一次报全，绝不静默错译（spec 用户故事 3）

**Independent Test**: 提交 broken.mo 等非法样例集，核对诊断位置/错误码/退出码 2 且零生成物（quickstart 场景 C）

### Tests for User Story 3 ⚠️（先写，确认失败）

- [X] - [X] T030 [P] [US3] 多错误恢复单元测试（实现说明：覆盖合并至 tests/unit/test_parser.cpp 的 MultipleSyntaxErrors 与 UnsupportedConstructs 用例）
- [X] T031 [P] [US3] CLI 错误路径集成测试：空文件 MC0001、双模型 MC0002、未声明标识符（quickstart broken.mo）均 exit 2 且不产生输出目录，在 tests/integration/test_cli_errors.cpp

### Implementation

- [X] T032 [US3] 解析器 panic-mode 错误恢复（分号边界同步、跳过至可恢复点继续收集）：src/parser/parser.cpp
- [X] T033 [US3] 结构级检查：空/仅注释文件 MC0001、单文件多模型 MC0002：src/parser/parser.cpp
- [X] T034 [US3] 不支持构造的关键字级早期拒绝（package/import/algorithm/when/if-方程/connect/reinit 等 → 定位到关键字起点）：src/parser/parser.cpp
- [X] T035 [US3] 运行期发散路径：RK4 检测非有限值 → stderr 含当前时间提示 → 删除半成品 CSV → 退出码 3：src/runtime/rk4.hpp；测试接入 tests/integration/test_divergence.cpp（构造发散模型 examples/models/diverge.mo）

**Checkpoint**: 全部用户故事独立可用；SC-004 可度量

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 跨故事收尾与门禁固化

- [X] T036 一键质量门禁脚本：clang-format 校验 + clang-tidy + 构建 + ctest，scripts/check.sh
- [X] T037 [P] CLI `--version` 输出版本号后 exit 0：src/cli/main.cpp
- [X] T038 [P] SC-003 性能验证：程序化生成 ~500 行合成模型，断言端到端转换 <10 秒，在 tests/integration/test_performance.cpp
- [X] T039 文档收尾：README 链接 quickstart.md/spec.md；FR-001..010 ↔ 任务/测试覆盖对照表附于 plan.md 末尾
- [X] T040 最终合规演练：依次执行 quickstart.md 场景 A–D 并记录结果（宪法合规审查证据）

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 Setup**: 无依赖，立即开始（T002/T003 可并行）
- **Phase 2 Foundational**: 依赖 Phase 1；T005–T009 文件互不冲突可并行；完成前 BLOCKS 所有故事
- **Phase 3 US1 (MVP)**: 依赖 Phase 2；测试任务 T010–T013 先于实现 T014–T020；T021/T022 收口
- **Phase 4 US2**: 依赖 Phase 3 的 CLI/代码生成（T020）——比对对象是 US1 产物；T027/T028 模型编写可与比对器开发交错
- **Phase 5 US3**: 依赖 Phase 2（诊断基建）；建议在 US1 后进行以复用真实解析路径；T035 依赖 T018
- **Phase 6 Polish**: 依赖全部故事完成

### User Story Dependencies

- **US1**: 仅依赖 Foundational —— 无跨故事依赖
- **US2**: 消费 US1 的 CLI 与生成程序，但比对器/参考脚本本身独立可测（pytest 层）
- **US3**: 复用 US1 解析器代码路径，但验收（非法输入拒绝）不依赖 US2

### Parallel Opportunities

- Phase 2 内 T005–T009 五路并行
- US1 测试四件套 T010–T013 并行先行
- US2 内 T023→T024 与 T025、T027 三线并行
- US3 内 T030/T031 测试先行两路并行
- 跨故事：单人按序执行；多人时 US2/US3 可在 US1 MVP 验证后并行推进

---

## Parallel Example: User Story 1

```bash
# 先并行写出全部失败测试：
Task: "T010 解析器单元测试 tests/unit/test_parser.cpp"
Task: "T011 语义单元测试 tests/unit/test_semantic.cpp"
Task: "T012 金样基架 tests/golden/test_golden_cooling.cpp"
Task: "T013 运行时单元测试 tests/unit/test_runtime.cpp"
# 再按依赖链实现：T008/T009 → T014 → T015/T016 → T017 → T018 → T019 → T020
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 Setup → Phase 2 Foundational（阻塞门）
2. Phase 3 US1：测试先行 → 流水线实现 → T021 端到端绿灯
3. **STOP & VALIDATE**: 手工跑 quickstart 场景 A，确认"合法输入→可运行输出"
4. 此时产品已具备核心价值，可暂停评估

### Incremental Delivery

- +US2：获得回归安全网（此后每次改动都有 SC-002 兜底）
- +US3：补齐诊断体验与边缘防护（SC-004 归零静默错译）
- +Polish：门禁脚本化，宪法五原则全部有自动化证据

### Notes

- 每个任务完成即提交（commit），保持金样基线与实现同步演进
- 金样基线更新必须在 commit message 中说明理由（防止无意漂移破坏 FR-007）
- 任何任务无法在不违反宪法 II（缺测试）的情况下关闭，即视为未完成
