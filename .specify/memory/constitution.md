<!--
SYNC IMPACT REPORT
==================
Version change: (none, uninitialized template) -> 1.0.0
Bump rationale: Initial ratification of project constitution (new document).

Modified principles:
- [PRINCIPLE_1_NAME] -> I. 代码质量优先 (Code Quality First)
- [PRINCIPLE_2_NAME] -> II. 单元测试全覆盖 (Unit Test Coverage Mandatory)
- [PRINCIPLE_3_NAME] -> III. 仿真结果可验证 (Simulation Verifiability)
- [PRINCIPLE_4_NAME] -> IV. 简单性优先 (Simplicity First)
- [PRINCIPLE_5_NAME] -> V. 可复现性 (Reproducibility)

Added sections:
- 质量标准 (Quality Standards)
- 开发工作流 (Development Workflow)
- Governance

Removed sections:
- None (template example comments removed after instantiation)

Follow-up TODOs:
- None. All placeholders resolved.
-->

# pocmodelica Constitution

## Core Principles

### I. 代码质量优先 (Code Quality First)

代码质量是不可妥协的底线。所有合并进主分支的代码 MUST 满足：

- 遵循项目统一的命名、结构与风格约定（Modelica 代码遵循 Modelica Name Convention）。
- 无重复逻辑：相同行为 MUST 抽取为共享函数/模型，禁止复制粘贴式扩展。
- 复杂度必须有明确理由；无法解释其存在必要性的代码 MUST 删除。
- 每次变更 MUST 经过审查（人工或工具辅助），未通过静态检查的代码禁止合入。

理由：本项目为模型验证型 POC，质量低劣的建模代码会导致仿真结果不可信，
返工成本远高于一次性写好。

### II. 单元测试全覆盖 (Unit Test Coverage Mandatory)

所有功能（所有新增或修改的功能）MUST 具有对应的单元测试，无例外：

- 新增功能 MUST 在实现的同时交付单元测试；只有实现没有测试的提交禁止合入。
- 缺陷修复 MUST 先添加能复现该缺陷的失败测试，再修复。
- 测试 MUST 可自动化运行（脚本一键执行），禁止依赖手动验证。
- 测试失败时禁止合入代码；跳过（skip）测试必须附带书面理由并在后续迭代中消除。

理由：单元测试是回归安全网，也是功能正确性的可执行规格说明。

### III. 仿真结果可验证 (Simulation Verifiability)

作为 Modelica 建模 POC，每个模型/子系统 MUST 可被独立验证：

- 每个模型 MUST 有明确的输入、输出与预期行为描述。
- 关键模型 MUST 配备对照用例（基准值或解析解），并通过自动化断言比对。
- 无法验证正确性的模型不得作为其他功能的依赖基础。

### IV. 简单性优先 (Simplicity First)

从最简单可行的方案开始（Start Simple, YAGNI）：

- 禁止在缺乏当前需求的情况下引入抽象层、配置项或可选机制。
- 每个问题优先选择最小实现；扩展能力等到真实需求出现时再增加。
- 依赖数量保持最小，新增第三方依赖 MUST 说明不可替代的理由。

### V. 可复现性 (Reproducibility)

任何人在任何时间 MUST 能复现构建与仿真结果：

- 工具链版本、依赖库版本 MUST 显式固定并纳入版本控制。
- 仿真结果 MUST 可通过单一命令重新生成。
- 影响结果的任何变更（参数、求解器设置、依赖版本）MUST 在提交说明中注明。

## 质量标准 (Quality Standards)

- **测试门禁**：CI（或本地等效脚本）中单元测试全绿是合入的硬性前置条件。
- **静态检查**：代码 MUST 通过项目配置的 linter/格式化检查后方可提交审查。
- **命名与文档**：公共模型、函数与库 MUST 包含说明用途的文档注释；
  仅内部使用的代码至少需要一行意图说明。
- **禁止事项**：禁止提交注释掉的死代码、调试残留输出、未使用的变量与导入。

## 开发工作流 (Development Workflow)

1. **明确目标**：每项工作开始前先写清要实现的行为及其验收方式。
2. **测试先行**：先编写会失败的单元测试，确认其准确刻画预期行为。
3. **最小实现**：以让测试通过所需的最小改动完成实现。
4. **重构**：在测试保护下消除重复、改善命名与结构，随后重跑全部测试。
5. **审查与合入**：提交前自查宪法合规性（原则 I–V 全部满足）；审查者重点核查测试质量与模型可验证性。

## Governance

- 本宪法是项目内所有开发实践的最高准则，与其他约定冲突时以本文件为准。
- 修订流程：提出修订提案 → 说明影响范围与迁移方案 → 记录于本文件的
  Sync Impact Report → 更新版本号后方可生效。
- 版本策略遵循语义化版本：不兼容的原则删改升 MAJOR；新增原则或实质性
  扩充升 MINOR；措辞澄清与非语义修订升 PATCH。
- 所有 PR 与代码审查 MUST 核对是否符合本宪法；无法证明合规的变更不予合入。
- 运行时开发指引参见各特性的 plan/spec 文档；当其与本宪法冲突时，以本宪法为准。

**Version**: 1.0.0 | **Ratified**: 2026-08-24 | **Last Amended**: 2026-08-24
