#pragma once
// mcruntime：确定性 CSV 结果写出器（contracts/csv-result-schema.md）。
// 数值 %.17g、C locale（不调用 setlocale）、UTF-8 无 BOM、'\n' 换行。
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace mcruntime {

class CsvWriter {
public:
  CsvWriter(const std::string &path, const std::vector<std::string> &columns) : path_(path) {
    file_ = std::fopen(path.c_str(), "w");
    if (file_ == nullptr) {
      throw std::runtime_error("无法打开结果文件: " + path);
    }
    for (size_t i = 0; i < columns.size(); ++i) {
      if (i > 0)
        std::fputc(',', file_);
      std::fputs(columns[i].c_str(), file_);
    }
    std::fputc('\n', file_);
  }

  ~CsvWriter() {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }

  CsvWriter(const CsvWriter &) = delete;
  CsvWriter &operator=(const CsvWriter &) = delete;

  void writeRow(double time, const std::vector<double> &values) {
    writeNumber(time);
    for (double v : values) {
      std::fputc(',', file_);
      writeNumber(v);
    }
    std::fputc('\n', file_);
  }

  const std::string &path() const { return path_; }

private:
  void writeNumber(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    std::fputs(buf, file_);
  }

  std::string path_;
  std::FILE *file_ = nullptr;
};

} // namespace mcruntime
