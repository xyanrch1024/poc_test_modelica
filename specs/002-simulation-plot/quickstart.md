# Quickstart: 内嵌绘图端到端验证指南

**Feature**: 002-simulation-plot
目标：证明"带 `--plot` 运行生成的仿真程序 → CSV + PNG 同时产出 → 图像正确且确定"
全链路可用（spec 用户故事 1/2/3 的可执行版本）。
实现细节见 [plan.md](./plan.md) 与 [contracts/](./contracts/)。

## 前置条件

已完成 001 的构建（`./build/src/modelicac` 可用）；无新增外部依赖（FR-010）。

## 场景 A: Hello 模型出图全链路（用户故事 1）

1. 转换并构建（若已有 example/hello_gen 可跳过转换，需重新生成以获得绘图开关）：
   ```bash
   ./build/src/modelicac translate example/hello.mo -o example/hello_gen
   cmake -S example/hello_gen -B example/hello_gen/build -DCMAKE_BUILD_TYPE=Release
   cmake --build example/hello_gen/build
   ```
2. 带开关运行：
   ```bash
   cd example/hello_gen && ./build/Hello --plot
   ```
   ✅ 预期：exit 0；stdout 末尾出现 `plot: <…>/Hello_result.png`；
   目录内同时存在 `Hello_result.csv` 与 `Hello_result.png`。
3. 校验图像：
   ```bash
   file Hello_result.png    # PNG, 960 x 540
   ```
4. 确定性复核：再跑一次，两次文件逐字节一致：
   ```bash
   cp Hello_result.png /tmp/p1.png && ./build/Hello --plot >/dev/null
   cmp /tmp/p1.png Hello_result.png && echo DETERMINISTIC
   ```

## 场景 B: 列筛选与输出路径（用户故事 2）

```bash
# 多变量模型：仅绘制部分列 + 自定义路径
./build/AlgChain --plot --plot-columns=s,z --plot-output=/tmp/alg_sz.png
```
✅ 预期：exit 0；摘要行含 `/tmp/alg_sz.png`；图像图例恰为 s、z 两条曲线。

## 场景 C: 失败诊断（用户故事 3）

```bash
./build/AlgChain --plot --plot-columns=not_a_var
```
✅ 预期：仿真与 CSV 正常完成（exit 0）；
stderr 出现 `[plot]` 前缀诊断并列出可用列名；stdout 摘要为 `plot: skipped (…)`；
不产生任何 .png 文件。

## 场景 D: 默认行为回归（001 契约）

```bash
cd example/hello_gen && ./build/Hello
```
✅ 预期：exit 0；仅生成 CSV；stdout 无任何 plot 行；不产生 .png。

## 场景 E: 测试门禁（宪法 II）

```bash
ctest --test-dir build --output-on-failure
```
✅ 预期：既有用例全绿 + 新增绘图单元/金样/集成用例全绿。
