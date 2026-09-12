#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace mcdc {

// 稳定错误码（specs/001-modelica-cpp-compiler/contracts/diagnostics-contract.md）
// 数值部分与契约表一致；新增码 MUST 追加于契约表尾部后在此同步。
enum class Code : int {
  StructureEmptyFile = 1,           // MC0001
  StructureMultipleModels = 2,      // MC0002
  SymbolDuplicate = 101,            // MC0101
  SymbolUndeclared = 102,           // MC0102
  SymbolConstantMissingValue = 103, // MC0103
  ExprUnsupported = 201,            // MC0201
  ExprUnknownFunction = 202,        // MC0202
  ExprBadDerPlacement = 203,        // MC0203
  EqUnderdetermined = 301,          // MC0301
  EqOverdetermined = 302,           // MC0302
  EqAlgebraicLoop = 303,            // MC0303
  InitMissingStart = 304,           // MC0304
  ExperimentInvalid = 401,          // MC0401
  UnsupportedConstruct = 501,       // MC0501
  ExprIfConditionNotBoolean = 204,  // MC0204
  ExprIfBranchTypeMismatch = 205,   // MC0205
  EqIfBranchMismatch = 305,         // MC0305
  EqIfMissingElse = 306,            // MC0306（预留：本期不触发）
  Internal = 9001,                  // MC9001
};

struct Location {
  std::string file;
  int line = 0;
  int col = 0;
};

enum class Severity { Error, Warning };

struct Diagnostic {
  Severity severity;
  Location location;
  Code code;
  std::string message;
};

class DiagnosticCollector {
public:
  void addError(Location loc, Code code, std::string message);
  void addWarning(Location loc, Code code, std::string message);
  const std::vector<Diagnostic> &all() const { return diags_; }
  bool hasErrors() const;
  // 按 (file, line, col) 升序返回全部诊断。
  std::vector<Diagnostic> sorted() const;
  // 为未携带文件名的诊断（如词法阶段）补全文件名。
  void fillMissingFile(const std::string &file);

private:
  void add(Severity severity, Location loc, Code code, std::string message);

  std::vector<Diagnostic> diags_;
};

// 渲染为 "[error] file.mo:3:7: 消息 [MC0102]" 格式。
std::string render(const Diagnostic &d);

} // namespace mcdc
