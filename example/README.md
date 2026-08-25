# example 演示目录

`hello.mo` 为最简示例模型。两个产物子目录均为可再生的生成物（不入库）：

```bash
# 本项目编译器：转换 → 构建 → 运行（CSV+PNG）
../build/src/modelicac translate hello.mo -o hello_gen
cmake -S hello_gen -B hello_gen/build && cmake --build hello_gen/build
cd hello_gen && ./build/Hello --plot

# OpenModelica 对照编译
cp hello.mo hello_omc/ && cd hello_omc && omc sim.mos && ./Hello -r Hello_result.csv
