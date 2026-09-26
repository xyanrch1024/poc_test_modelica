#include "simcode/create.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>

#include "eqir/ir.h"
#include "simcode/emit_expr.h"
#include "simcode/names.h"

namespace mcdc::simcode {
namespace {

std::string literalFromToken(const Token &tok, double value) {
  if (tok.kind == TokKind::Int)
    return tok.lexeme;
  return formatRealLiteral(value);
}

RefMap buildContRefs(const SimCode &sc) {
  RefMap refs;
  for (const auto &p : sc.parameters)
    refs[p.name] = "v." + p.cppName;
  for (const auto &s : sc.states)
    refs[s.name] = "v." + s.cppName;
  for (const auto &a : sc.algebraics)
    refs[a.name] = "v." + a.cppName;
  return refs;
}

void collectAlgebraics(const eqir::EqModule &mod, const SymbolTable &table, bool initMode,
                       std::vector<SimVar> *out, std::set<std::string> *seen) {
  auto push = [&](const std::string &name) {
    if (!seen->insert(name).second)
      return;
    if (initMode && eqir::isDerUnknownName(name))
      return; // der 局部量，不进 Vars
    SimVar v;
    v.name = name;
    if (initMode) {
      v.cppName = mapInitUnknownToCpp(name);
      v.cppType = "double";
    } else {
      const SymbolInfo *info = table.find(name);
      v.cppName = mapToCppIdentifier(name);
      v.cppType = declTypeName(info ? info->type : ast::Component::DeclType::Real);
    }
    v.comment = "algebraic: " + name;
    out->push_back(std::move(v));
  };

  for (const auto &step : mod.schedule) {
    if (step.kind == eqir::StepKind::Assign)
      push(step.assign.var);
    else {
      for (const auto &tv : step.solve.tearVars)
        push(tv);
    }
  }
  for (const auto &[name, _] : mod.aliases) {
    (void)_;
    push(name);
  }
}

void fillScheduleEqs(const eqir::EqModule &mod, const RefMap &refs, std::vector<SimEq> *eqs) {
  for (const auto &step : mod.schedule) {
    SimEq eq;
    if (step.kind == eqir::StepKind::Assign) {
      eq.kind = SimEq::Kind::Assign;
      auto it = refs.find(step.assign.var);
      if (it == refs.end())
        throw std::logic_error("schedule assign 未映射: " + step.assign.var);
      eq.lhsCpp = it->second;
      eq.rhsCpp = emitExpr(*step.assign.rhs, refs);
    } else {
      eq.kind = SimEq::Kind::Nonlinear;
      for (const auto &name : step.solve.tearVars) {
        auto it = refs.find(name);
        if (it == refs.end())
          throw std::logic_error("tear var 未映射: " + name);
        eq.tearRefs.push_back(it->second);
      }
      for (const auto *r : step.solve.residuals)
        eq.residualCpp.push_back(emitExpr(*r, refs));
    }
    eqs->push_back(std::move(eq));
  }
}

void fillAliasEqs(const eqir::EqModule &mod, const RefMap &refs, bool initMode,
                  std::vector<SimEq> *eqs) {
  for (const auto &[name, info] : mod.aliases) {
    SimEq eq;
    eq.kind = SimEq::Kind::Assign;
    auto lit = refs.find(name);
    if (lit == refs.end())
      throw std::logic_error("alias 未映射: " + name);
    eq.lhsCpp = lit->second;
    if (info.isConst) {
      eq.rhsCpp = formatRealLiteral(info.constValue);
    } else {
      std::string rhs;
      if (initMode) {
        const std::string canonCpp = mapInitUnknownToCpp(info.canonical);
        if (canonCpp.rfind("der_", 0) == 0)
          rhs = canonCpp;
        else
          rhs = "v." + canonCpp;
      } else {
        rhs = "v." + mapToCppIdentifier(info.canonical);
      }
      if (info.scale == -1.0)
        eq.rhsCpp = "-" + rhs;
      else
        eq.rhsCpp = rhs;
    }
    eqs->push_back(std::move(eq));
  }
}

} // namespace

SimCode createSimCode(const ast::Model &model, const SymbolTable &table, const eqir::EqModule &cont,
                      const eqir::EqModule &init, DiagnosticCollector &diags) {
  SimCode sc;
  sc.modelName = model.nameTok.lexeme;

  std::set<std::string> statesSet(cont.states.begin(), cont.states.end());
  std::set<std::string> visiting;

  for (const auto &comp : model.components) {
    const std::string &name = comp.nameTok.lexeme;
    switch (comp.kind) {
    case ast::Component::Kind::Constant:
    case ast::Component::Kind::Parameter: {
      SimVar v;
      v.name = name;
      v.cppName = mapToCppIdentifier(name);
      v.cppType = declTypeName(comp.type);
      v.comment = name;
      if (comp.value) {
        auto value = evalConstExpr(*comp.value, model, table, &visiting, diags);
        if (value) {
          if (comp.type == ast::Component::DeclType::Boolean)
            v.initLiteral = (*value != 0.0) ? "true" : "false";
          else
            v.initLiteral = literalFromToken(comp.value->token, *value);
        } else {
          v.initLiteral = "0";
        }
      } else {
        v.initLiteral = comp.type == ast::Component::DeclType::Boolean ? "false" : "0";
      }
      sc.parameters.push_back(std::move(v));
      break;
    }
    case ast::Component::Kind::Variable: {
      if (statesSet.count(name)) {
        SimVar st;
        st.name = name;
        st.cppName = mapToCppIdentifier(name);
        st.cppType = declTypeName(comp.type);
        st.comment = "state: " + name;
        st.index = static_cast<int>(sc.states.size());
        sc.states.push_back(std::move(st));
      }
      SimVar out;
      out.name = name;
      out.cppName = mapToCppIdentifier(name);
      out.cppType = declTypeName(comp.type);
      sc.outputs.push_back(std::move(out));
      break;
    }
    }
  }

  std::set<std::string> seenAlg;
  collectAlgebraics(cont, table, false, &sc.algebraics, &seenAlg);

  for (const auto &u : init.unknowns) {
    if (eqir::isDerUnknownName(u.name))
      sc.initDerLocals.push_back(mapInitUnknownToCpp(u.name));
  }

  RefMap contRefs = buildContRefs(sc);
  fillScheduleEqs(cont, contRefs, &sc.algebraicEquations);
  fillAliasEqs(cont, contRefs, false, &sc.aliasEquations);

  for (const auto &st : sc.states) {
    SimEq eq;
    eq.kind = SimEq::Kind::Assign;
    eq.lhsCpp = "d." + st.cppName;
    auto it = cont.stateRhs.find(st.name);
    if (it == cont.stateRhs.end() || it->second == nullptr)
      throw std::logic_error("缺少状态导数: " + st.name);
    eq.rhsCpp = emitExpr(*it->second, contRefs);
    sc.odeEquations.push_back(std::move(eq));
  }

  // 初始系统 refs：Vars 成员 + der 局部量
  RefMap initRefs = contRefs;
  for (const auto &u : init.unknowns) {
    if (eqir::isDerUnknownName(u.name))
      initRefs[u.name] = mapInitUnknownToCpp(u.name);
    else
      initRefs[u.name] = "v." + mapToCppIdentifier(u.name);
  }
  // 别名目标也可能是连续代数量
  for (const auto &[name, _] : init.aliases) {
    (void)_;
    if (!initRefs.count(name)) {
      if (eqir::isDerUnknownName(name))
        initRefs[name] = mapInitUnknownToCpp(name);
      else
        initRefs[name] = "v." + mapToCppIdentifier(name);
    }
  }

  for (const auto &[name, guess] : init.guesses) {
    InitGuess g;
    if (eqir::isDerUnknownName(name))
      g.lhsCpp = mapInitUnknownToCpp(name);
    else
      g.lhsCpp = "v." + mapToCppIdentifier(name);
    g.literal = formatRealLiteral(guess);
    sc.initGuesses.push_back(std::move(g));
  }

  fillScheduleEqs(init, initRefs, &sc.initialEquations);
  fillAliasEqs(init, initRefs, true, &sc.initialEquations);

  sc.startTime = cont.startTime;
  sc.stopTime = cont.stopTime;
  const double span = sc.stopTime - sc.startTime;
  long steps = static_cast<long>(std::llround(span / cont.interval));
  if (steps < 1)
    steps = 1;
  sc.steps = steps;
  return sc;
}

} // namespace mcdc::simcode
