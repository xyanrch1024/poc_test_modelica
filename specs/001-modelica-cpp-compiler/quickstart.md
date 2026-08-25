# Quickstart: 端到端验证指南

**Feature**: 001-modelica-cpp-compiler
目标：证明"Modelica 源码 → C++ 工程 → 运行 → CSV 轨迹 → 与 OpenModelica 参考比对"
全链路可用（spec 用户故事 1/2、SC-001/002 的最小可执行版本）。
实现细节见 [plan.md](./plan.md) 与 [contracts/](./contracts/)；本文只含验证步骤。

## 前置条件（固定版本见 research.md R9）

```bash
g++ --version          # ≥ 12, 支持 -std=c++20
cmake --version        # ≥ 3.24
python3 --version      # ≥ 3.11
omc --version          # OpenModelica ≥ 1.23（仅生成参考基线时需要）
```

## 场景 A: 首个模型闭环（用户故事 1）

1. **准备示例模型** `examples/models/cooling.mo`
   （牛顿冷却定律，解析解 x(t)=Tamb+(T0−Tamb)e^(−kt)，可人工核对）：

   ```modelica
   model Cooling
     parameter Real k = 0.5;
     parameter Real Tamb = 20;
     Real T(start = 90, fixed = true);
   equation
     der(T) = -k * (T - Tamb);
     annotation(experiment(StartTime = 0, StopTime = 5, Interval = 0.01));
   end Cooling;
   ```

2. **转换**：
   ```bash
   ./build/modelicac translate examples/models/cooling.mo -o /tmp/cooling_gen
   ```
   ✅ 预期：exit 0；`/tmp/cooling_gen/` 含 CMakeLists.txt 与模型源文件。

3. **构建生成物**：
   ```bash
   cmake -S /tmp/cooling_gen -B /tmp/cooling_gen/build && cmake --build /tmp/cooling_gen/build
   ```
   ✅ 预期：零错误零警告完成（FR-002）。

4. **运行**：
   ```bash
   cd /tmp && ./cooling_gen/build/Cooling
   ```
   ✅ 预期：exit 0；当前目录出现 `Cooling_result.csv`；
   表头 `time,T`；502 行（含表头与初始行）；t=5 时 T ≈ 25.75
   （解析解 20 + 70·e⁻²·⁵ ≈ 25.7459，容差内）。

## 场景 B: 批量回归验证（用户故事 2）

前置：`references/cooling_res.csv` 已由 omc 参考脚本生成并入库。

```bash
# 重新生成参考（可选，验证 omc 环境）
omc --version && omc references/gen_reference.mos

# 运行批量比对（内部依次执行场景 A 步骤 2–4 后逐用例比较）
python3 tools/regression/run_regression.py examples/models/references.cases
```

✅ 预期输出末行：`SUMMARY: N/N PASS`；任一 FAIL 时进程退出非零。

## 场景 C: 诊断路径（用户故事 3）

```bash
printf 'model Broken\n  Real x;\nequation\n  x = y + 1;\nend Broken;\n' > /tmp/broken.mo
./build/modelicac translate /tmp/broken.mo
```

✅ 预期：stderr 输出 `[error] /tmp/broken.mo:3:7: 使用未声明标识符 "y" [MC0102]`；
退出码 2；不产生任何输出目录。

## 场景 D: 测试全绿（宪法 II 门禁）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
```

✅ 预期：unit + golden + integration 全部通过，0 failed。
