# Implementation Plan: 仿真结果图形化表示

**Branch**: `002-simulation-plot` | **Date**: 2026-08-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/002-simulation-plot/spec.md`

## Summary

在生成的仿真程序中内嵌可选绘图能力：新增运行开关 `--plot`（含列筛选与输出路径选项），
仿真结束时在 CSV 同目录生成 `<模型名>_result.png` 轨迹图（标题/轴标签/图例齐全）；
零参数默认行为与 001 契约逐字节一致。

技术路线：全部以 C++20 标准库在运行时头文件中实现——内存 RGB 帧缓冲 +
Bresenham 直线光栅化 + 内嵌 5×7 点阵字体 + 手写 PNG 编码器
（stored-deflate zlib 流，自算 CRC32/Adler32，天然确定性）。
数据侧做等宽桶降采样保持视觉形状；绘图失败只出诊断，不改变退出码。

## Technical Context

**Language/Version**: C++20（与编译器本体一致；GCC ≥12 / Clang ≥15）

**Primary Dependencies**: 零第三方依赖（FR-010）——仅 C++ 标准库；
测试沿用 GoogleTest v1.14.0 与既有 CTest 基建

**Storage**: N/A（读内存结果序列；写 PNG 文件）

**Testing**: GoogleTest 单元测试（PNG 结构/CRC、坐标映射、降采样、字体、像素探针）+
金样 PNG 基线 + 集成测试（--plot 全链路、默认行为回归、错误分支）

**Target Platform**: Linux x86-64 开发验收环境；生成程序保持跨主流平台可移植

**Project Type**: compiler/cli 的运行时扩展（header-only，随生成工程分发）

**Performance Goals**: 20 万行数据下 --plot 总耗时较纯 CSV 模式增加 ≤10 秒（FR-007）；
图像 ≤5 MB（SC-004）

**Constraints**: 确定性输出（同输入同开关 → 逐字节同 PNG，FR-006）；
绘图失败不得影响退出码语义（FR-005）；零第三方依赖（FR-010）

**Scale/Scope**: 运行时新增约 800–1200 行（帧缓冲/PNG/字体/降采样/渲染）；
代码生成器模板扩展约 100 行；测试约 600–900 行

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| 原则 | 设计落点 | 状态 |
|------|----------|------|
| I. 代码质量优先 | 绘图拆为五个单一职责模块（framebuffer/font/png/mapping/render）；clang-format/tidy 门禁沿用 | ✅ PASS |
| II. 单元测试全覆盖 | 每个模块独立单测 + PNG 金样 + 集成全链路；开关各分支覆盖（FR-009） | ✅ PASS |
| III. 仿真结果可验证 | 曲线数据源自同一内存序列；首/中/末像素探针断言（SC-002 自动化） | ✅ PASS |
| IV. 简单性优先 | 无配置面：固定尺寸/配色/降采样参数常量；stored-deflate 避免 zlib 依赖 | ✅ PASS |
| V. 可复现性 | PNG 无时间戳元数据块；固定调色板与绘制顺序 → 逐字节确定（FR-006） | ✅ PASS |

**Gate 结论**: 无违规项，进入 Phase 0。

## Project Structure

### Documentation (this feature)

```text
specs/002-simulation-plot/
├── plan.md              # 本文件
├── research.md          # Phase 0 输出：技术决策记录
├── data-model.md        # Phase 1 输出：绘图内部数据模型
├── quickstart.md        # Phase 1 输出：端到端验证指南
├── contracts/
│   ├── plot-cli-contract.md    # 运行开关契约（--plot 及其选项）
│   └── png-output-contract.md  # PNG 产物契约（结构/尺寸/确定性）
└── tasks.md             # Phase 2 输出（/speckit.tasks 生成）
```

### Source Code (repository root)

```text
src/runtime/                # 新增 header-only 绘图运行时（随生成工程分发）
├── plot_types.hpp          # PlotRequest/TimeSeries/PlotLayout 等类型
├── plot_framebuffer.hpp    # RGB 帧缓冲与 Bresenham 光栅化
├── plot_font.hpp           # 内嵌 5×7 ASCII 点阵字体
├── png_encoder.hpp         # 手写 PNG 编码（stored deflate + CRC32/Adler32）
├── plot_mapping.hpp        # 数据→像素映射、自动量程、等宽桶降采样
└── plot_render.hpp         # 组装渲染：坐标轴/网格/曲线/图例/标题

src/codegen/codegen.cpp     # 生成 main() 模板扩展：开关解析与绘图调用
tests/unit/test_plot_*.cpp  # 各模块单元测试 + test_png_encoder.cpp
tests/golden/hello_plot/    # PNG 金样基线
tests/integration/          # --plot 端到端 / 默认行为回归 / 错误分支
```

**Structure Decision**: 绘图能力完全落在 header-only 运行时层，
代码生成器只负责透传开关与调用一个 `plot::render_and_save(...)` 入口；
编译器本体（parser/semantic 等）零改动。

## Complexity Tracking

> Constitution Check 全部通过，无需要辩护的违规项。

## 附录: FR ↔ 实现/测试覆盖对照（T023）

| 需求 | 实现位置 | 测试证据 |
|------|----------|----------|
| FR-001 内嵌开关触发 | codegen.cpp 模板 main 解析 + plot_render.hpp | PlotE2E 双用例（开/关行为） |
| FR-002 输出路径+摘要行 | 模板选项逻辑 + RenderStats | PlotColumns.Custom / E2E stdout 断言 |
| FR-003 列筛选 | render_and_save 列校验 | PlotColumns.Single/Multi、UnknownColumn |
| FR-004 标题/轴/图例 | plot_render.hpp 版面组装 | RenderToFramebuffer.TitleLegendAxis |
| FR-005 诊断+退出码中立 | [plot] 前缀分支 | PlotErrors 三用例（含 exit 3 叠加） |
| FR-006 确定性 | 无元数据块+固定调色板 | PngEncoder.Deterministic、GoldenPlot 金样 |
| FR-007 降采样性能 | plot_mapping.hpp 桶极值法 | PlotPerf.200kRows（増时≤10s） |
| FR-008 非有限点跳过 | 映射期剔除计数 | MapSeries.SkipsNonFinite + 摘要统计 |
| FR-009 全功能测试覆盖 | tests/unit/test_plot_* + integration | ctest 96 用例全绿 |
| FR-010 零第三方依赖 | 手写 PNG 编码器（stored deflate） | PngEncoder 结构/CRC 单测；无新依赖引入 |
