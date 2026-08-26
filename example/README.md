# example 演示目录

`hello.mo` 为最简示例模型。两个产物子目录均为可再生的生成物（不入库）：

```bash
# 本项目编译器：转换 → 构建 → 运行（CSV+PNG）
../build/src/modelicac translate hello.mo -o hello_gen
cmake -S hello_gen -B hello_gen/build && cmake --build hello_gen/build
cd hello_gen && ./build/Hello --plot

# OpenModelica 对照编译
cp hello.mo hello_omc/ && cd hello_omc && omc sim.mos && ./Hello -r Hello_result.csv

## RoomHeating 暖通演示（hvac.mo，简化版）

单节点供暖模型：房间空气热容 + 渗风损失 + 恒功率热源（500W 加热 + 200W 得热）。
初始 15°C 冷态，室外 −5°C，仿真 3 小时（步长 1s? 实际输出间隔 30s，361 点）。
时间常数 τ≈42 分钟，温度从 15°C 单调升向稳态 ≈24.17°C。

```bash
../build/src/modelicac translate hvac.mo -o hvac_gen
cmake -S hvac_gen -B hvac_gen/build && cmake --build hvac_gen/build
cd hvac_gen && ../build/RoomHeating --plot --plot-columns=TRoom
```

### 与 OpenModelica 的交叉验证结果（omc 1.27.0 / DASSL）

| 项目 | 本项目(RK4 h=30s) | omc(DASSL) |
|------|-------------------|------------|
| 全程 361 点逐点比对 | 最大偏差 **4.1e-05 °C** | — |
| 终态 TRoom | 24.042°C | 24.042°C |

线性方程对两种求解器均近乎精确；验收容差 ±0.05°C 远宽于实际偏差。

### 已知经验

1. 子集要求 `der(x)` 单独在左侧——`C*der(x)=rhs` 需改写为 `der(x)=rhs/C`；
2. omc 结果表头带引号且末行时间戳重复，比对前需裁剪；
3. sim.mos 的 stopTime 必须与模型 experiment 注释同步修改，否则参考数据被截断。
