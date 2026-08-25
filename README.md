# pocmodelica — Modelica → C++ 编译器 (POC)

将首期 Modelica 子集（连续时间 ODE 模型，见
[spec](specs/001-modelica-cpp-compiler/spec.md) FR-004）翻译为可构建、可运行、
确定性输出的 C++ 仿真程序。

## 工具链基线（固定版本，宪法原则 V）

| 组件 | 版本要求 |
|------|----------|
| GCC / Clang | ≥12 / ≥15（`-std=c++20`） |
| CMake | ≥3.24 |
| GoogleTest | v1.14.0（FetchContent 自动拉取） |
| clang-format / clang-tidy | 17.x |
| Python / pytest | ≥3.11 / ≥8（`tools/regression/requirements.txt`） |
| OpenModelica（参考基准生成） | ≥1.23（仅 `references/` 基线再生成时需要） |

## 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 快速使用

```bash
./build/modelicac translate examples/models/cooling.mo -o /tmp/cooling_gen
cmake -S /tmp/cooling_gen -B /tmp/cooling_gen/build && cmake --build /tmp/cooling_gen/build
/tmp/cooling_gen/build/Cooling   # 生成 Cooling_result.csv（零命令行参数）
```

### 仿真绘图（002 特性）

生成的程序内嵌可选绘图开关：

```bash
cd /tmp/cooling_gen && ./build/Cooling --plot
# → CSV 照常产出，同时生成 Cooling_result.png（960×540 轨迹图）
./build/Cooling --plot --plot-columns=T --plot-output=/tmp/T.png  # 列筛选与自定义路径
```

完整验证流程见 [quickstart](specs/001-modelica-cpp-compiler/quickstart.md)；
绘图特性见 [002 quickstart](specs/002-simulation-plot/quickstart.md)。

## 文档索引

- 规格: specs/001-modelica-cpp-compiler/spec.md
- 计划: specs/001-modelica-cpp-compiler/plan.md
- 任务: specs/001-modelica-cpp-compiler/tasks.md
- 契约: specs/001-modelica-cpp-compiler/contracts/
