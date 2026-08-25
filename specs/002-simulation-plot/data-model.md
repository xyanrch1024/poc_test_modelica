# Phase 1 数据模型: 内嵌绘图运行时

**Feature**: 002-simulation-plot | **Date**: 2026-08-24
配套契约见 [contracts/](./contracts/)；验证指南见 [quickstart.md](./quickstart.md)。

## 渲染流水线与状态迁移

```text
仿真循环 ──采集──▶ TimeSeries 集 ──筛选/降采样/映射──▶ PixelPaths
   │                                                    │
   ▼ (发散 exit 3)                                 rasterize
   放弃绘图 + 说明                                    ▼
                                              Framebuffer ──encode──▶ PNG 文件
```

任何阶段失败：打印 `[plot]` 诊断 → 不写图像文件 → 主流程退出语义不变（FR-005）。

## 实体定义

### PlotRequest（绘图请求，来自运行开关）
| 字段 | 类型 | 规则 |
|------|------|------|
| enabled | bool | 由 `--plot` 置位；缺省 false |
| columns | vector<string> 或空 | 空=全部变量列；名字必须存在于输出序列，否则诊断+放弃 |
| outputPath | string | 缺省 `<模型名>_result.png`；父目录须可写 |

### TimeSeries（单变量时间序列）
| 属性 | 说明 |
|------|------|
| name | 源模型列名（图例显示原文） |
| times / values | 与网格等长；允许含非有限值（映射期剔除并计数） |

### PlotLayout（版面常量，编译期固定）
| 常量 | 值 | 用途 |
|------|-----|------|
| kWidth × kHeight | 960×540 | 画布像素 |
| 边距 L/R/T/B | 72/24/48/44 | 绘图区 = 内缩矩形 |
| kPalette[8] | 固定 RGB 表 | 曲线/图例色，按列序循环 |
| kMaxPointsPerPixel×2 | 绘图区宽×2 | 超过则触发桶降采样阈值 |

### AxisMapping（数据→像素）
| 规则 | 说明 |
|------|------|
| 自动量程 | 有限值 min/max 外扩 5%；min==max → 区间 ±1 |
| 线性映射 | 数据坐标 → 绘图区整数像素；越界点裁剪 |
| 刻度 | 约 5 条主刻度（nice numbers），横轴标数值、纵轴标数值 |

### DownsampledSeries
- 等宽桶 min/max 对序列（保序）；点数 ≤ 阈值时原样透传；
- 输出为折线顶点序列（PixelPath）。

### Framebuffer
- RGB24 字节数组；操作：`clear(白)`、`draw_line(Bresenham)`、`fill_rect`、`draw_text(font,scale)`；
- 越界绘制调用为安全无操作（防御性，不崩溃）。

### PngEncoder
| 规则 | 说明 |
|------|------|
| 结构 | 签名 + IHDR(960,540,8bit,truecolor,非隔行) + IDAT(stored deflate) + IEND |
| 校验 | 每块 CRC32 自算；zlib 流含 Adler32 |
| 确定性 | 无文本元数据块；同帧缓冲逐字节同输出（FR-006） |

### RenderStats（结束摘要依据）
| 字段 | 来源 |
|------|------|
| skippedPoints | 映射期剔除的非有限点总数 |
| drawnSeries / skippedSeries | 成功绘制数 / 全坏点放弃数 |
| outputPath | 实际写出路径（摘要行 `plot: <path>` 使用） |

### 绘图失败分类（诊断前缀 `[plot]`）
| 触发 | 行为 |
|------|------|
| columns 含未知名 | 诊断列出可用列名；不出图 |
| 输出目录不可建/不可写 | 诊断含路径原因；不出图 |
| 序列全为非有限值 | 该序列跳过；若全部如此则不出图并注明 |
| 发散退出叠加 | 放弃绘图，说明一行（CSV 契约为先） |
