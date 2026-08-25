# Phase 0 研究记录: Modelica 到 C++ 编译器

**Feature**: 001-modelica-cpp-compiler | **Date**: 2026-08-24
**状态**: 全部 NEEDS CLARIFICATION 已解决

## R1. 编译器本体的实现语言

- **Decision**: C++20。
- **Rationale**: 与生成物同语言，编译器与运行时可共享数值类型与测试基建，单一工具链降低
  构建复杂度（宪法 IV）；GoogleTest 提供成熟的单元测试与金样测试能力（宪法 II）；
  C++20 概念/范围特性可保证解析器与代码生成器的类型安全（宪法 I）。
- **Alternatives considered**:
  - Rust：内存安全更优，但引入第二套工具链且团队生态与生成端 C++ 分裂；
  - Python：迭代最快，但大型编译器的静态检查与重构支持弱，与"代码质量优先"门禁成本高；
  - OCaml：编译器经典选择，但招致最小众的工具链维护负担。

## R2. 解析策略

- **Decision**: 手写递归下降解析器（无解析器生成器依赖）。
- **Rationale**: 首期子集文法小而稳定（见 contracts/subset-grammar.md）；手写可完全控制
  错误恢复与"一次报出多个错误"（spec 用户故事 3 场景 3），行列号追踪直接内建；
  零第三方依赖符合简单性与可复现性原则。
- **Alternatives considered**: ANTLR4（功能强但引入运行时依赖与生成文件，诊断控制间接）；
  Bison/flex（错误恢复定制困难，产物不可读）。

## R3. 方程语义的翻译模型

- **Decision**: 编译期做方程级分析：①统计方程数 vs 未知量数实现欠定/超定检查；
  ②以变量为节点、方程为边构建依赖图；③`der(x)` 出现的变量标记为状态量；
  ④代数环（强连通分量 >1 节点或自环的非线性耦合）→ 拒绝并报诊断；
  ⑤对无环代数方程做拓扑排序，生成按序赋值语句；微分方程交给运行时积分器。
- **Rationale**: 子集限定为 ODE 型连续系统，拓扑排序 + RK4 是语义等价的最简实现路径，
  且环检测天然满足 spec 边缘用例"代数环必须被检出"。
- **Alternatives considered**: Pantelides 展开/DAE 求解（超出首期范围，复杂度不可辩护）；
  数值迭代解代数环（引入非确定性收敛行为，违反 FR-007）。

## R4. 数值积分方法（规划阶段遗留决策）

- **Decision**: 固定步长经典四阶 Runge-Kutta（RK4），步长 = (StopTime−StartTime)/间隔数，
  间隔数由实验注释 Interval 推导。
- **Rationale**: 固定步长保证逐位可复现（FR-007）与回归比对的稳定采样网格；
  四阶精度足以覆盖首期验证模型的容差要求（SC-002）；仅标准库即可实现（FR-002 默认零依赖）。
- **Alternatives considered**: 前向欧拉（精度不足，同容差下需过小步长）；
  自适应步长 DASSL/CVODE 类（结果受浮点环境扰动更大，且需第三方库）。
  刚性问题出现时按 FR-002 政策引入固定版本的开源库，属后续迭代。

## R5. 单元测试框架与测试策略

- **Decision**: GoogleTest v1.14.0（CMake FetchContent 固定 tag）。
  三层：单元测试（lexer/parser/semantic/codegen 各模块）、
  金样测试（codegen 输出与 tests/golden/ 基线比对，保证 FR-007 确定性可回归）、
  集成测试（CTest 驱动：转换→构建生成程序→运行→断言 CSV 内容）。
- **Rationale**: 金样测试把"相同输入相同产物"变成可执行断言；
  CTest 统一编排三层测试，CI 与本地命令一致（宪法 II、V）。
- **Alternatives considered**: doctest/Catch2（均可，但 GoogleTest 生态与 CTest 集成最成熟）。

## R6. 参考基准生成与比对

- **Decision**: 以 omc 脚本批处理生成参考：
  `omc` 中执行 `simulate(<Model>, startTime=…, stopTime=…, numberOfIntervals=…, outputFormat="csv")`
  → 得到 `<Model>_res.csv`（含 time 列）。参考 CSV 入库至 `references/`。
  比对器 `tools/regression/compare.py` 按"公共时间网格最近邻插值 + 相对/绝对混合容差"
  逐变量比较，输出量化最大偏差与通过/失败结论；比对器自身用 pytest 测试。
- **Rationale**: OpenModelica 免费且脚本化（已核实其 simulate API 支持 outputFormat="csv"）；
  参考入库使回归不依赖即时联网/装环境，符合可复现性。
- **Alternatives considered**: Dymola 等商业工具（许可成本）；解析解基准
 （仅覆盖极少数模型，作为 examples 的补充校验而非主机制）。

## R7. 生成程序的第三方依赖政策

- **Decision**: v1 运行时仅用 C++ 标准库；政策上允许未来引入少量开源数学/线性代数库，
  届时 MUST 在 CMake 中固定版本并在文档声明（落实 FR-002）。
- **Rationale**: 当前子集（RK4 + 初等函数）标准库完全够用；预留政策口径避免将来返工。
- **Alternatives considered**: 立即引入 Eigen（当前无使用场景，违反 YAGNI）。

## R8. 诊断与退出码设计

- **Decision**: 文本人读格式 `[error] file.mo:12:3: message [MC0102]`（详见
  contracts/diagnostics-contract.md）；退出码 0=成功、1=用法/内部错误、2=存在诊断错误。
  收集全部可发现错误后一次性输出（用户故事 3 场景 3）。
- **Rationale**: 稳定的错误码前缀便于自动化归因；退出码三分法满足 FR-006 且足够简单。
- **Alternatives considered**: JSON 结构化诊断（首期消费者是人+简单 CI，推迟）。

## R9. 工具链与版本固定清单

| 组件 | 固定版本 |
|------|----------|
| GCC / Clang | ≥12 / ≥15（-std=c++20） |
| CMake | ≥3.24 |
| GoogleTest | v1.14.0（FetchContent tag） |
| clang-format / clang-tidy | 17.x（.clang-format 入库） |
| Python / pytest | ≥3.11 / ≥8.x（requirements.txt 固定） |
| OpenModelica（参考基准） | ≥1.23（版本记录于 references/VERSIONS.md） |

- **Decision**: 上表为唯一允许的开发/构建环境基线，写入仓库 README 与 CI。
- **Rationale**: 宪法 V 要求显式固定并纳入版本控制。
