# 契约: 编译器命令行接口 (CLI)

**稳定性**: 首期冻结；破坏性变更需升 constitution MAJOR 级评审。

## 命令

```text
modelicac translate <input.mo> [-o <outdir>] [--help] [--version]
```

| 参数 | 必选 | 说明 |
|------|------|------|
| `translate` | 是 | 子命令：翻译 Modelica 源文件为 C++ 工程 |
| `<input.mo>` | 是 | Modelica 源文件路径（唯一事实输入，FR-001） |
| `-o <outdir>` | 否 | 输出目录，默认 `./<model名>_gen` |
| `--version` | — | 打印版本后退出 0 |

## 退出码

| 码 | 含义 |
|----|------|
| 0 | 转换成功，输出目录已生成（FR-006 可区分性） |
| 1 | 用法错误 / 输入文件不可读 / 编译器内部错误 |
| 2 | 源文件存在诊断错误（error 级），不产出任何生成物 |

## 行为规则

- 失败时（exit 1/2）不得创建或残留输出目录（spec 用户故事 3 场景 1）。
- 多个诊断一次性全部报告后再以 exit 2 结束。
- stdout 仅输出成功摘要与 ValidationReport 类信息；所有诊断走 stderr。

## 构建生成物（编译器不代劳，契约规定标准方式）

```bash
cmake -S <outdir> -B <outdir>/build && cmake --build <outdir>/build
```
