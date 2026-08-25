# Tasks: 仿真结果图形化表示

**Input**: Design documents from `/specs/002-simulation-plot/`

**Prerequisites**: plan.md ✅ | spec.md ✅ | research.md ✅ | data-model.md ✅ | contracts/ ✅

**Tests**: 强制包含——宪法原则 II + FR-009 要求全部功能有自动化测试；
每个故事阶段先写失败测试再实现。

**Organization**: 按用户故事分组（US1=P1 基础出图、US2=P2 列筛选与输出、US3=P3 失败诊断）。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成依赖）
- **[Story]**: 所属用户故事（仅故事阶段标注）
- 所有路径相对仓库根目录

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 绘图运行时的文件骨架就位（实现留空，供后续测试先行编译）

- [X] T001 创建六个运行时头文件骨架（include guard + 命名空间 mcruntime::plot）：src/runtime/plot_types.hpp、plot_font.hpp、png_encoder.hpp、plot_framebuffer.hpp、plot_mapping.hpp、plot_render.hpp

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 四个独立叶子模块——全部用户故事的阻塞前置

**⚠️ CRITICAL**: 本阶段完成前不得开始任何用户故事

### Tests for Foundational ⚠️（先写，确认失败）

- [X] T002 [P] 字体单元测试：ASCII 32–126 全集逐字符渲染不越界、5×7 尺寸常量正确，在 tests/unit/test_plot_font.cpp
- [X] T003 [P] PNG 编码器单元测试：签名/IHDR/IDAT/IEND 结构解析、每块 CRC32 重算一致、zlib 流 Adler32 校验、同输入两次编码逐字节一致，在 tests/unit/test_png_encoder.cpp
- [X] T004 [P] 帧缓冲单元测试：set_pixel/越界安全、fill_rect、Bresenham 水平/垂直/斜线像素探针，在 tests/unit/test_plot_framebuffer.cpp

### Implementation

- [X] T005 实现实体类型：PlotRequest/TimeSeries/PlotLayout 常量（960×540、边距、8 色调色板、降采样阈值）/RenderStats：src/runtime/plot_types.hpp
- [X] T006 [P] 实现 5×7 ASCII 点阵字体（constexpr 字形表 32–126 + 取模接口）：src/runtime/plot_font.hpp
- [X] T007 [P] 实现手写 PNG 编码器（RGB24→PNG：CRC32/Adler32、stored deflate、原子写临时名+rename）：src/runtime/png_encoder.hpp
- [X] T008 [P] 实现 RGB 帧缓冲与 Bresenham 光栅化（clear/draw_line/fill_rect/draw_text 越界安全）：src/runtime/plot_framebuffer.hpp

**Checkpoint**: 叶子模块就绪且单测全绿，可开始用户故事

---

## Phase 3: User Story 1 - 一键将仿真结果转为轨迹图 (Priority: P1) 🎯 MVP

**Goal**: `--plot` 开关使生成的仿真程序在 CSV 同目录产出完整轨迹图；零参数行为零变化（spec 用户故事 1）

**Independent Test**: Hello 生成程序带 `--plot` 运行 → exit 0、CSV 照旧、`Hello_result.png` 存在且曲线与数据一致（quickstart 场景 A）

### Tests for User Story 1 ⚠️（先写，确认失败）

- [X] T009 [P] [US1] 映射单元测试：自动量程外扩 5%、min==max 退化 ±1、等宽桶 min/max 降采样保形保序、非有限点剔除计数，在 tests/unit/test_plot_mapping.cpp
- [X] T010 [P] [US1] 渲染组装像素探针测试：线性序列首/中/末数据点落在折线像素上、标题区与图例区存在非白像素、坐标轴绘制，在 tests/unit/test_plot_render.cpp
- [X] T011 [P] [US1] 集成测试骨架：--plot 全链路（CSV+PNG 同时产出、摘要行 `plot: <path>`）与默认行为回归（无 plot 行、无 png），在 tests/integration/test_plot_e2e.cpp

### Implementation

- [X] T012 [US1] 实现自动量程/线性映射/桶降采样/坏点统计：src/runtime/plot_mapping.hpp（依赖 T002–T009）
- [X] T013 [US1] 实现渲染组装（白底/网格/轴与刻度/标题 2×/图例色块+列名/调色板循环/单点方块标记）：src/runtime/plot_render.hpp
- [X] T014 [US1] 扩展生成模板 main()：三开关解析（=连接式）、TimeSeries 采集、render_and_save 调用、摘要行输出；分发新头文件至生成工程 mcruntime/：src/codegen/codegen.cpp
- [X] T015 [US1] 金样基线：Hello --plot 的 PNG 入库 tests/golden/hello_plot/ + 逐字节比对测试 tests/golden/test_golden_plot.cpp（FR-006）
- [X] T016 [US1] 验证集成用例通过并手动执行 quickstart 场景 A/D 记录结果

**Checkpoint**: User Story 1 可独立演示（MVP 达成）

---

## Phase 4: User Story 2 - 选择变量与控制输出 (Priority: P2)

**Goal**: 列筛选与自定义输出路径（spec 用户故事 2）

**Independent Test**: AlgChain 以 `--plot-columns=s,z --plot-output=/tmp/x.png` 运行 → 图像恰两条曲线、路径生效

### Tests for User Story 2 ⚠️（先写，确认失败）

- [X] T017 [P] [US2] 集成测试：单列图例唯一、多列同图、自定义相对/绝对路径写入、父目录自动创建，在 tests/integration/test_plot_columns.cpp

### Implementation

- [X] T018 [US2] 实现列筛选（逗号分隔解析、按名匹配输出序列）与输出路径处理（缺省命名、mkdir parents）：src/codegen/codegen.cpp 模板选项逻辑

**Checkpoint**: 用户故事 1 与 2 均独立可用

---

## Phase 5: User Story 3 - 失败输入的清晰诊断 (Priority: P3)

**Goal**: 绘图失败只出 `[plot]` 诊断、零残缺产物、退出码中立；发散叠加放弃绘图（spec 用户故事 3 + 边缘用例）

**Independent Test**: 未知列名运行 → CSV 照常、stderr 有可用列清单、无 png、exit 仍为 0（quickstart 场景 C）

### Tests for User Story 3 ⚠️（先写，确认失败）

- [X] T019 [P] [US3] 错误分支集成测试：未知列名诊断含可用列清单、不可写输出路径诊断、发散+--plot 叠加（exit 3、CSV 在、无 png、摘要 skipped），在 tests/integration/test_plot_errors.cpp
- [X] T020 [P] [US3] 坏点统计单元测试：inf/nan 混合列剔除计数、全坏序列跳过标注，并入 tests/unit/test_plot_mapping.cpp

### Implementation

- [X] T021 [US3] 实现 RenderStats 摘要行三态（path/skipped(原因)）与全部失败分支诊断（含发散叠加放弃逻辑）：src/codegen/codegen.cpp 模板 + src/runtime/plot_render.hpp

**Checkpoint**: 全部用户故事独立可用；SC-003 可度量

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 性能门禁、文档与合规收尾

- [X] T022 [P] SC-004 性能集成测试：合成 20 万行模型 --plot 总耗时较纯 CSV 増时 ≤10s 且 png ≤5MB，在 tests/integration/test_plot_perf.cpp
- [X] T023 [P] 文档更新：README 增加 --plot 用法与 002 quickstart/契约链接；plan.md 追加 FR-001..010 ↔ 任务/测试对照表
- [X] T024 最终演练：依次执行 quickstart 场景 A–E 并记录结果（宪法合规证据）；全量 ctest + pytest 绿灯确认

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 Setup**: 无依赖，立即开始
- **Phase 2 Foundational**: 依赖 T001；T002–T004 测试先行，T005–T008 四模块并行实现；完成前 BLOCKS 所有故事
- **Phase 3 US1 (MVP)**: 依赖 Phase 2；T009/T010 测试先行 → T012/T013 → T014（模板接线）→ T015 金样 → T016 收口
- **Phase 4 US2**: 依赖 US1 的模板接线（T014）
- **Phase 5 US3**: 依赖 US1（T014）；T021 与 US2 无冲突可并行推进
- **Phase 6 Polish**: 依赖全部故事完成

### User Story Dependencies

- **US1**: 仅依赖 Foundational —— 无跨故事依赖
- **US2/US3**: 均构建于 US1 的开关解析与渲染入口之上，但彼此独立、可并行
- 发散叠加用例（T019）复用 001 的 diverge.mo，不依赖 US2

### Parallel Opportunities

- Phase 2：T002–T004 三路测试并行 → T005–T008 四路实现并行
- US1：T009/T010/T011 三路测试先行并行
- US2（T017–T018）与 US3（T019–T021）可在 US1 MVP 后并行推进
- Phase 6：T022/T023 并行

---

## Parallel Example: User Story 1

```bash
# 先并行写出全部失败测试：
Task: "T009 映射单元测试 tests/unit/test_plot_mapping.cpp"
Task: "T010 渲染像素探针测试 tests/unit/test_plot_render.cpp"
Task: "T011 e2e 骨架 tests/integration/test_plot_e2e.cpp"
# 再按依赖链实现：T012 → T013 → T014 → T015 金样 → T016 验证
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 Setup → Phase 2 Foundational（阻塞门）
2. Phase 3 US1：映射+渲染+模板接线 → 金样冻结 → e2e 绿灯
3. **STOP & VALIDATE**: 手工跑 quickstart 场景 A/D，确认"开关出图、默认不变"
4. 此时绘图主干已可用，可暂停评估

### Incremental Delivery

- +US2：筛选与输出控制（大模型可读性）
- +US3：诊断完备与发散叠加防护（SC-003 归零静默失败）
- +Polish：SC-004 性能门禁固化、文档与合规演练

### Notes

- 每个任务完成即提交（commit）；金样 PNG 更新必须在 commit message 说明理由
- 绘图代码全部 header-only，注意 <cmath>/<cstdint> 自包含 include
- 任何任务无法在不违反宪法 II（缺测试）的情况下关闭，即视为未完成
