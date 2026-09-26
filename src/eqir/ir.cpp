#include "eqir/ir.h"

namespace mcdc::eqir {

const Unknown *findUnknown(const EqModule &m, const std::string &name) {
  for (const auto &u : m.unknowns) {
    if (u.name == name)
      return &u;
  }
  return nullptr;
}

Unknown *findUnknownMutable(EqModule &m, const std::string &name) {
  for (auto &u : m.unknowns) {
    if (u.name == name)
      return &u;
  }
  return nullptr;
}

Equation *findEquation(EqModule &m, size_t id) {
  for (auto &eq : m.equations) {
    if (eq.id == id)
      return &eq;
  }
  return nullptr;
}

const Equation *findEquation(const EqModule &m, size_t id) {
  for (const auto &eq : m.equations) {
    if (eq.id == id)
      return &eq;
  }
  return nullptr;
}

} // namespace mcdc::eqir
