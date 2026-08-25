# Phase 0 研究记录: 内嵌绘图运行时

**Feature**: 002-simulation-plot | **Date**: 2026-08-24
**状态**: 全部技术决策已解决（无遗留 NEEDS CLARIFICATION）

## R1. PNG 编码方案（零依赖核心难点）

- **Decision**: 手写 PNG 编码器：签名 + IHDR + IDAT + IEND 四块；
  zlib 流使用 **stored（未压缩）deflate 块**，CRC32 与 Adler32 自实现
  （各约 25 行查表/直接计算）。真彩色 RGB8、非隔行。
- **Rationale**: stored-deflate 完全规避压缩器实现与依赖；
  PNG 规范允许 stored 块；尺寸固定 960×540 时图像 ≈1.5 MB，
  远低于 SC-004 的 5 MB 上限。字节输出天然确定（无时间戳/无元数据块）。
- **Alternatives considered**:
  - 引入 zlib/libpng：违反 FR-010 零依赖；
  - 实现真正的 deflate 压缩：复杂度高且对 5MB 目标无必要；
  - BMP 格式：非规格要求的 PNG（spec Assumptions 首期锁定 PNG）。

## R2. 光栅化与帧缓冲

- **Decision**: `std::vector<unsigned char>` RGB24 帧缓冲（960×540×3），
  Bresenham 直线算法画曲线/坐标轴，矩形填充用于图例色块。
- **Rationale**: 标准库容器 + 整数算法即可完成；Bresenham 确定且无需浮点累积误差处理；
  内存占用 ~1.55 MB/帧，可接受。
- **Alternatives considered**: 抗锯齿渲染（引入浮点混合，破坏逐字节确定性的可解释性，POC 无必要）。

## R3. 文本渲染（标题/轴标签/图例）

- **Decision**: 内嵌 5×7 ASCII 点阵字体（constexpr 数组，覆盖可打印 ASCII 32–126），
  1× 缩放用于轴刻度与图例，2× 缩放用于标题；不支持非 ASCII 字符——
  非 ASCII 列名在图例中以 `?` 占位序列渲染并计入诊断提示。
- **Rationale**: 点阵字体是零依赖文本渲染的唯一轻量路径；
  数据模型中变量名来自源模型（Modelica 标识符本为 ASCII），
  特殊字符属防御性边缘用例（spec Edge Cases）而非主流程。
- **Alternatives considered**: 矢量字体/STB 类库（违反零依赖）；跳过文本（不满足 FR-004）。

## R4. 坐标映射与降采样

- **Decision**:
  - 自动量程：取所选序列的有限值 min/max，两端外扩 5%；
    time 轴同规则；min==max 时退化为 ±1 区间（水平线居中，覆盖退化边缘用例）。
  - 降采样：当单序列点数 > 绘图区宽度 ×2 时做**等宽桶 min/max 对**压缩
    （每桶保留最小/最大两点，保序输出），视觉形状不变且确定性成立。
  - 非有限点：映射阶段直接剔除并计数（FR-008），全部点非有限的序列不绘制并在摘要注明。
- **Rationale**: 桶极值法是波形保持降采样的标准做法；全程整数/有限浮点比较，结果稳定。
- **Alternatives considered**: LTTB 算法（形状更优但选择逻辑含浮点权重，确定性与测试成本更高）。

## R5. 配色与版面

- **Decision**: 固定 8 色调色板（高区分度 RGB 常量），按列声明序循环取色；
  白底黑轴；版面常量：960×540、边距 left=72/right=24/top=48/bottom=44；
  网格线浅灰（仅主刻度）；>8 条曲线时循环复用颜色并以图例区分。
- **Rationale**: 固定常量满足"美学不做定制接口"假设与确定性要求；
  8 色覆盖典型验证模型的变量规模。
- **Alternatives considered**: HSV 自动生成色（相邻系列区分度不稳定）。

## R6. 运行开关设计

- **Decision**: 三个新增可选参数（详见 contracts/plot-cli-contract.md）：
  - `--plot`：启用绘图（默认关闭）；
  - `--plot-columns=<a,b,c>`：逗号分隔列筛选（缺省=全部变量列）；
  - `--plot-output=<path>`：输出路径（缺省=`<模型名>_result.png` 于 CSV 同目录）。
  参数解析在生成的 main() 中完成（前缀匹配，顺序无关）；
  未识别参数仍按 001 契约忽略并提示后正常退出（保持既有行为）。
- **Rationale**: 与"零参数默认行为不变"约束正交；`=` 连接式选项避免多 token 解析复杂度。
- **Alternatives considered**: 环境变量触发（不可见性好）；独立配置文件（过度设计）。

## R7. 失败语义与发散叠加

- **Decision**: 绘图失败（选项非法/列名缺失/路径不可写）→ 打印 `[plot] ...` 前缀诊断到 stderr，
  不产出图像文件，**退出码沿用主仿真结果**（0 或发散时的 3）；
  发散退出时若绘图开关启用：放弃绘图并打印说明（CSV 契约为先，spec 边缘用例）。
  结束摘要新增一行：`plot: <path>` 或 `plot: skipped (<原因>)` 或 `plot: off`。
- **Rationale**: 满足 FR-005"退出码中立"；摘要行让自动化可探测绘图状态。
- **Alternatives considered**: 绘图失败改退出码（破坏 001 契约，被 FR-005 禁止）。

## R8. 测试策略

- **Decision**:
  - 单元：PNG 编码器（结构解析+CRC 重算校验+金样）、映射（量程/退化/降采样性质）、
    字体（ASCII 全集渲染不越界）、帧缓冲（像素探针）、渲染组装（首/中/末三点落线断言）；
  - 金样：Hello --plot 的整幅 PNG 基线（tests/golden/hello_plot/）；
  - 集成：默认行为回归（无 --plot 零差异）、--plot 全链路、错误分支三例、发散叠加。
- **Rationale**: 像素探针 + 结构校验能在不引入图像解码依赖的前提下给出强断言；
  金样兜住整体回归。
- **Alternatives considered**: 用 Python PIL 解码验证（给测试引入新依赖，探针法足够）。

## R9. 版本与文档

- **Decision**: 无新增外部组件版本；运行时文件随生成工程分发，
  在 generated-program-contract 中追加绘图相关条款引用。
- **Rationale**: 零依赖设计使版本表无需变更（宪法 V）。
