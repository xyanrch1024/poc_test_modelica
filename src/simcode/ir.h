#pragma once
#include <string>
#include <vector>

namespace mcdc::simcode {

// 仿真代码 IR（对齐 OpenModelica SimCode 职责的子集）：
// EqIR/InitIR 调度结果在此降成「已 emit 的 C++ 片段」，模板只负责拼文件结构。

struct SimVar {
  std::string name;     // 源名
  std::string cppName;  // C++ 标识符
  std::string cppType;  // double / long long / bool
  std::string comment;  // Vars 成员行尾注释
  std::string initLiteral; // 仅 Parameter
  int index = -1;       // State 在 y[] 中的下标；其它为 -1
};

struct SimEq {
  enum class Kind { Assign, Nonlinear } kind = Kind::Assign;
  // Assign
  std::string lhsCpp;
  std::string rhsCpp;
  // Nonlinear（Newton）
  std::vector<std::string> tearRefs;     // 写入 x[] / 从 x[] 读回的 C++ 左值
  std::vector<std::string> residualCpp; // rr[i] = …
};

struct InitGuess {
  std::string lhsCpp;
  std::string literal;
};

struct SimCode {
  std::string modelName;
  std::string generatorVersion = "modelicac 0.1.0";

  std::vector<SimVar> parameters;
  std::vector<SimVar> states;
  std::vector<SimVar> algebraics;
  std::vector<SimVar> outputs;

  std::vector<SimEq> algebraicEquations;
  std::vector<SimEq> aliasEquations; // apply_aliases / init 末尾别名
  std::vector<SimEq> odeEquations;   // d.state = rhs

  std::vector<std::string> initDerLocals; // e.g. der_T
  std::vector<InitGuess> initGuesses;
  std::vector<SimEq> initialEquations; // 含 init 别名

  double startTime = 0.0;
  double stopTime = 0.0;
  long steps = 1;
};

} // namespace mcdc::simcode
