# Implementation Plan: Modelica 到 C++ 的编译器系统

**Branch**: `001-modelica-cpp-compiler` | **Date**: 2026-08-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-modelica-cpp-compiler/spec.md`

## Summary

构建一个命令行编译器，将首期 Modelica 子集（单模型文件、连续时间微分/代数方程、
参数与初始条件、实验设置注释、初等表达式）翻译为可构建、可运行、确定性的 C++ 仿真程序。
生成的程序零命令行参数运行，从模型实验注释读取终止时间与输出间隔，
以固定步长 RK4 推进仿真并将结果轨迹写入 CSV 文件。
配套批量回归机制：以 OpenModelica 生成的参考 CSV 为基准做容差比对。

技术路线：C++20 实现编译器本体（词法→递归下降解析→语义检查→方程依赖图分析
（环检测+拓扑排序）→代码生成），生成物链接一个小型仅依赖标准库的运行时头文件库；
测试用 GoogleTest（单元+金样+集成），回归比对器为独立 Python 小工具。

## Technical Context

**Language/Version**: 编译器本体 C++20（GCC ≥12 或 Clang ≥15）；回归比对工具 Python ≥3.11

**Primary Dependencies**: 编译器：GoogleTest v1.14.0（FetchContent 固定版本）、CMake ≥3.24；
生成程序运行时：仅 C++ 标准库；参考基准：OpenModelica ≥1.23（omc 脚本化调用）

**Storage**: N/A（纯转换工具；输入 .mo 文件，输出 C++ 工程目录与 CSV 结果文件）

**Testing**: GoogleTest 单元测试 + 金样（golden file）代码生成测试 +
CTest 集成测试（构建并运行生成程序）；pytest 覆盖回归比对器；
clang-format/clang-tidy 强制静态检查

**Target Platform**: Linux x86-64 开发与验收环境（WSL Ubuntu）；
生成代码仅用标准库以保持跨主流平台可移植

**Project Type**: compiler/cli（单一仓库、单一主项目）

**Performance Goals**: ≤500 行模型端到端转换 <10 秒（SC-003，实际预期 <2 秒）

**Constraints**: 确定性输出（FR-007：同输入同产物）；诊断必含 file:line（FR-006）；
生成程序零参数运行（FR-003）；第三方依赖必须固定版本（FR-002）

**Scale/Scope**: 首期子集编译器，预估 5k–10k 行编译器代码 + 运行时数百行；
≥20 个代表性验证模型（SC-001）

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| 原则 | 设计落点 | 状态 |
|------|----------|------|
| I. 代码质量优先 | clang-format/clang-tidy 门禁；递归下降解析器等模块职责单一；共享运行时头文件避免重复 | ✅ PASS |
| II. 单元测试全覆盖 | 每个语言构造至少一个解析/语义/金样测试（FR-009）；缺陷修复附回归用例；CTest 全绿为合入门禁 | ✅ PASS |
| III. 仿真结果可验证 | 每个受支持构造对应 OpenModelica 参考用例；批量比对器量化偏差（FR-010） | ✅ PASS |
| IV. 简单性优先 | 手写解析器而非 ANTLR/Bison；运行时仅标准库；单项目结构；无插件/配置层 | ✅ PASS |
| V. 可复现性 | 工具链与依赖全部固定版本；确定性排序（有序容器遍历）；固定步长 RK4 数值确定性；omc 版本随文档固定 | ✅ PASS |

**Gate 结论**: 无违规项，进入 Phase 0。

## Project Structure

### Documentation (this feature)

```text
specs/001-modelica-cpp-compiler/
├── plan.md              # 本文件
├── research.md          # Phase 0 输出：技术决策记录
├── data-model.md        # Phase 1 输出：内部数据模型
├── quickstart.md        # Phase 1 输出：端到端验证指南
├── contracts/           # Phase 1 输出：外部契约
│   ├── cli-contract.md            # 编译器命令行契约
│   ├── diagnostics-contract.md    # 诊断信息格式与退出码
│   ├── subset-grammar.md          # 支持子集文法（EBNF）
│   ├── generated-program-contract.md # 生成程序行为契约
│   └── csv-result-schema.md       # CSV 结果格式契约
└── tasks.md             # Phase 2 输出（/speckit.tasks 生成）
```

### Source Code (repository root)

```text
src/
├── cli/                 # 命令行入口、退出码映射
├── lexer/               # 词法分析（含行列号追踪）
├── ast/                 # AST 节点定义
├── parser/              # 递归下降解析器
├── semantic/            # 符号表、方程/未知量配平、代数环检测、拓扑排序
├── codegen/             # C++ 代码生成器
├── diagnostics/         # 诊断收集与渲染
└── runtime/             # 供生成代码包含的运行时头文件（RK4、CSV 写出）

tools/regression/        # 回归比对器（Python）：compare.py 及其测试
tests/
├── unit/                # 各模块单元测试（GoogleTest）
├── golden/              # 代码生成金样基线
└── integration/         # 端到端：转换→构建→运行→断言（CTest）

examples/models/         # 代表性验证模型集（.mo）
references/              # OpenModelica 生成的参考 CSV 基线（入库）
```

**Structure Decision**: 采用单一项目布局。编译器各阶段为独立库目标（便于隔离测试），
最终链接为一个可执行文件；runtime 为 header-only 目标被生成代码直接包含；
回归比对作为开发侧工具放在 tools/，不进入产品发布面。

## Complexity Tracking

> Constitution Check 全部通过，无需要辩护的违规项。

## 附录: FR ↔ 实现/测试覆盖对照（T039）

| 需求 | 实现位置 | 测试证据 |
|------|----------|----------|
| FR-001 输入→C++ 工程 | src/cli/pipeline.cpp, src/codegen/ | test_end_to_end (转换成功) |
| FR-002 可构建+依赖固定 | 生成 CMakeLists 模板, codegen.cpp | EndToEnd 三用例(零修改构建) |
| FR-003 CSV 轨迹+零参数运行 | src/runtime/csv_writer.hpp, 生成 main | CoolingModelFullLoop / ParamVariation |
| FR-004 子集界定 | contracts/subset-grammar.md, parser | ParserValid.* 全产生式 |
| FR-005 超界拒绝 MC0501 | parser reportUnsupported | UnsupportedConstructsReportMC0501 |
| FR-006 file:line 诊断+退出码 | diagnostics/, cli/main.cpp | CliErrors.*(exit 1/2 区分) |
| FR-007 确定性 | 有序遍历+%.17g+金样 | Rk4.BitwiseDeterministic, GoldenCooling |
| FR-008 标识符溯源 | plan.mapToCppIdentifier + 注释 | PlanBuild.MapsIdentifiers(switch_) |
| FR-009 全构造测试覆盖 | tests/unit/*, golden, integration | ctest 53 用例全绿 |
| FR-010 批量验证 | tools/regression/run_regression.py | regression_suite (CTest), pytest 11 |
| SC-001 ≥20 代表模型 | examples/models/case_01..20 | 全部 translate 通过(本仓库验证) |
| SC-003 <10s 转换 | — | Performance.Translates500LineModelUnderTenSeconds |
| SC-004 零静默错译 | FR-005/FR-006 机制 | CliErrors + Divergence(exit 3) |
