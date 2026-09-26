#pragma once
#include <string>
#include <unordered_map>

#include "ast/ast.h"

namespace mcdc::simcode {

// sourceName → 已带前缀的 C++ 左值/右值（如 "v.T" 或 "der_T"）
using RefMap = std::unordered_map<std::string, std::string>;

std::string emitExpr(const ast::Expr &e, const RefMap &refs);

} // namespace mcdc::simcode
