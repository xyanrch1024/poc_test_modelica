#pragma once
// 集成测试公共工具：命令执行、文件读取、CSV 解析。
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <filesystem>

namespace itest {

namespace fs = std::filesystem;

// fork+exec 显式等待，避免 system() 的信号屏蔽语义干扰。
inline int run(const std::string &cmd) {
  const pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char *>(nullptr));
    _exit(127);
  }
  if (pid < 0)
    return -1;
  int status = 0;
  if (waitpid(pid, &status, 0) < 0)
    return -1;
  if (!WIFEXITED(status))
    return -(1000 + WTERMSIG(status));
  return WEXITSTATUS(status);
}

// 单引号包裹路径（测试内路径受控，不含单引号）。
inline std::string q(const std::string &path) {
  return "'" + path + "'";
}

inline bool readFile(const std::string &path, std::string *out) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

inline std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (!line.empty())
      lines.push_back(line);
  }
  return lines;
}

inline void cleanDir(const std::string &dir) {
  fs::remove_all(dir);
}

// 绝对路径（cd 后相对路径失效，统一转绝对）。
inline std::string absPath(const std::string &p) {
  return fs::absolute(p).string();
}

} // namespace itest
