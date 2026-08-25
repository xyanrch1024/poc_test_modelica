#include "diagnostics/diagnostic.h"

#include <cstdio>
#include <stdexcept>

namespace mcdc {

void DiagnosticCollector::addError(Location loc, Code code, std::string message) {
  add(Severity::Error, std::move(loc), code, std::move(message));
}

void DiagnosticCollector::addWarning(Location loc, Code code, std::string message) {
  add(Severity::Warning, std::move(loc), code, std::move(message));
}

bool DiagnosticCollector::hasErrors() const {
  return std::any_of(diags_.begin(), diags_.end(),
                     [](const Diagnostic &d) { return d.severity == Severity::Error; });
}

std::vector<Diagnostic> DiagnosticCollector::sorted() const {
  auto copy = diags_;
  std::sort(copy.begin(), copy.end(), [](const Diagnostic &a, const Diagnostic &b) {
    if (a.location.file != b.location.file)
      return a.location.file < b.location.file;
    if (a.location.line != b.location.line)
      return a.location.line < b.location.line;
    return a.location.col < b.location.col;
  });
  return copy;
}

void DiagnosticCollector::fillMissingFile(const std::string &file) {
  for (auto &d : diags_) {
    if (d.location.file.empty())
      d.location.file = file;
  }
}

void DiagnosticCollector::add(Severity severity, Location loc, Code code, std::string message) {
  diags_.push_back(Diagnostic{severity, std::move(loc), code, std::move(message)});
}

std::string render(const Diagnostic &d) {
  const char *severity = d.severity == Severity::Error ? "error" : "warning";
  char buf[32];
  std::snprintf(buf, sizeof(buf), "MC%04d", static_cast<int>(d.code));
  return "[" + std::string(severity) + "] " + d.location.file + ":" +
         std::to_string(d.location.line) + ":" + std::to_string(d.location.col) + ": " + d.message +
         " [" + buf + "]";
}

} // namespace mcdc
