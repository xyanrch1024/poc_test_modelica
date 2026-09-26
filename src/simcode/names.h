#pragma once
#include <optional>
#include <set>
#include <string>

#include "ast/ast.h"
#include "diagnostics/diagnostic.h"
#include "semantic/symbols.h"

namespace mcdc {

std::string mapToCppIdentifier(const std::string &name);
std::string mapInitUnknownToCpp(const std::string &name);
std::string formatRealLiteral(double v);
std::string declTypeName(ast::Component::DeclType type);

std::optional<double> evalConstExpr(const ast::Expr &expr, const ast::Model &model,
                                    const SymbolTable &table, std::set<std::string> *visiting,
                                    DiagnosticCollector &diags);

} // namespace mcdc
