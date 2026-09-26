#include "codegen/codegen.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#ifndef MC_RUNTIME_SRC_DIR
#error "MC_RUNTIME_SRC_DIR 必须由构建系统定义（指向 src/runtime）"
#endif
#ifndef MC_TEMPLATES_DIR
#error "MC_TEMPLATES_DIR 必须由构建系统定义（指向 templates）"
#endif

namespace mcdc {

namespace {

constexpr const char *kRuntimeSrcDir = MC_RUNTIME_SRC_DIR;
constexpr const char *kTemplatesDir = MC_TEMPLATES_DIR;

const std::vector<std::string> kRuntimeFiles = {
    "rk4.hpp",
    "csv_writer.hpp",
    "newton.hpp",
};

bool read_whole_file(const std::string &path, std::string *out) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

std::string renderEqSteps(const std::vector<simcode::SimEq> &eqs) {
  std::string body;
  for (const auto &eq : eqs) {
    if (eq.kind == simcode::SimEq::Kind::Assign) {
      body += "  " + eq.lhsCpp + " = " + eq.rhsCpp + ";\n";
    } else {
      const size_t n = eq.tearRefs.size();
      body += "  {\n";
      body += "    double x[" + std::to_string(n) + "] = {";
      for (size_t i = 0; i < n; ++i) {
        if (i)
          body += ", ";
        body += eq.tearRefs[i];
      }
      body += "};\n";
      body += "    auto residual = [&](const double* xx, double* rr) {\n";
      for (size_t i = 0; i < n; ++i) {
        body += "      " + eq.tearRefs[i] + " = xx[" + std::to_string(i) + "];\n";
      }
      for (size_t i = 0; i < n; ++i) {
        body += "      rr[" + std::to_string(i) + "] = " + eq.residualCpp[i] + ";\n";
      }
      body += "    };\n";
      body += "    mcruntime::NewtonOpts opts;\n";
      body += "    opts.tol = 1e-10;\n";
      body += "    opts.maxIter = 50;\n";
      body += "    mcruntime::newton(residual, x, " + std::to_string(n) + ", opts);\n";
      for (size_t i = 0; i < n; ++i) {
        body += "    " + eq.tearRefs[i] + " = x[" + std::to_string(i) + "];\n";
      }
      body += "  }\n";
    }
  }
  return body;
}

nlohmann::json varToJson(const simcode::SimVar &v) {
  return nlohmann::json{{"name", v.name},
                        {"cppName", v.cppName},
                        {"cppType", v.cppType},
                        {"comment", v.comment},
                        {"initLiteral", v.initLiteral},
                        {"index", v.index}};
}

nlohmann::json eqToJson(const simcode::SimEq &eq) {
  nlohmann::json j;
  j["lhsCpp"] = eq.lhsCpp;
  j["rhsCpp"] = eq.rhsCpp;
  return j;
}

nlohmann::json simCodeToJson(const simcode::SimCode &sc) {
  nlohmann::json data;
  data["generatorVersion"] = sc.generatorVersion;
  data["modelName"] = sc.modelName;
  data["startTime"] = formatRealLiteral(sc.startTime);
  data["stopTime"] = formatRealLiteral(sc.stopTime);
  data["steps"] = sc.steps;
  data["stateCount"] = sc.states.size();

  data["parameters"] = nlohmann::json::array();
  for (const auto &p : sc.parameters)
    data["parameters"].push_back(varToJson(p));
  data["states"] = nlohmann::json::array();
  for (const auto &s : sc.states)
    data["states"].push_back(varToJson(s));
  data["algebraics"] = nlohmann::json::array();
  for (const auto &a : sc.algebraics)
    data["algebraics"].push_back(varToJson(a));
  data["outputs"] = nlohmann::json::array();
  for (const auto &o : sc.outputs)
    data["outputs"].push_back(varToJson(o));

  data["aliasEquations"] = nlohmann::json::array();
  for (const auto &eq : sc.aliasEquations)
    data["aliasEquations"].push_back(eqToJson(eq));
  data["odeEquations"] = nlohmann::json::array();
  for (const auto &eq : sc.odeEquations)
    data["odeEquations"].push_back(eqToJson(eq));

  data["initDerLocals"] = sc.initDerLocals;
  data["initGuesses"] = nlohmann::json::array();
  for (const auto &g : sc.initGuesses)
    data["initGuesses"].push_back({{"lhsCpp", g.lhsCpp}, {"literal", g.literal}});

  data["algebraicBody"] = renderEqSteps(sc.algebraicEquations);
  data["initialBody"] = renderEqSteps(sc.initialEquations);

  std::string csvExtra;
  for (const auto &o : sc.outputs)
    csvExtra += ", \"" + o.name + "\"";
  data["csvExtraCols"] = csvExtra;

  std::string writeArgs;
  for (size_t i = 0; i < sc.outputs.size(); ++i) {
    if (i)
      writeArgs += ", ";
    writeArgs += "static_cast<double>(s." + sc.outputs[i].cppName + ")";
  }
  // 花括号放进数据，避免 inja 把 {{{ ... }}} 当 unescaped 吞掉 initializer 的 '{'
  data["writeRowList"] = "{" + writeArgs + "}";
  return data;
}

} // namespace

GeneratedFiles generateProject(const simcode::SimCode &sc, std::string *err) {
  GeneratedFiles files;
  try {
    inja::Environment env;
    env.set_trim_blocks(false);
    env.set_lstrip_blocks(false);
    const auto data = simCodeToJson(sc);

    const std::string modelTpl = std::string(kTemplatesDir) + "/cpp/model.cpp.inja";
    const std::string cmakeTpl = std::string(kTemplatesDir) + "/cpp/CMakeLists.txt.inja";
    files.emplace("model_" + sc.modelName + ".cpp", env.render_file(modelTpl, data));
    files.emplace("CMakeLists.txt", env.render_file(cmakeTpl, data));
  } catch (const std::exception &e) {
    if (err)
      *err = std::string("模板渲染失败: ") + e.what();
    return {};
  }

  for (const auto &name : kRuntimeFiles) {
    std::string content;
    if (!read_whole_file(std::string(kRuntimeSrcDir) + "/" + name, &content)) {
      if (err)
        *err = std::string("无法读取运行时源文件: ") + kRuntimeSrcDir + "/" + name;
      return {};
    }
    files.emplace("mcruntime/" + name, std::move(content));
  }
  return files;
}

bool writeProject(const GeneratedFiles &files, const std::string &outDir, std::string *err) {
  namespace fs = std::filesystem;
  try {
    fs::create_directories(outDir + "/mcruntime");
    for (const auto &[name, content] : files) {
      const std::string path = outDir + "/" + name;
      fs::create_directories(fs::path(path).parent_path());
      std::ofstream out(path, std::ios::binary);
      if (!out) {
        if (err)
          *err = "无法写入文件: " + path;
        return false;
      }
      out.write(content.data(), static_cast<std::streamsize>(content.size()));
      if (!out) {
        if (err)
          *err = "写入失败: " + path;
        return false;
      }
    }
    return true;
  } catch (const std::exception &e) {
    if (err)
      *err = e.what();
    return false;
  }
}

} // namespace mcdc
