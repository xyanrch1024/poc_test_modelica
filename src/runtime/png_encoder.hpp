#pragma once
// mcruntime::plot —— 手写 PNG 编码器（research R1，FR-006/FR-010）。
// 真彩色 RGB8、非隔行；zlib 流使用 stored deflate 块；
// CRC32/Adler32 自实现；不含任何文本元数据块 → 逐字节确定。
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace mcruntime::plot {

namespace detail {

inline const std::uint32_t* crc_table() {
  static std::uint32_t table[256];
  static bool init = [] {
    for (std::uint32_t n = 0; n < 256; ++n) {
      std::uint32_t c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[n] = c;
    }
    return true;
  }();
  (void)init;
  return table;
}

inline std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
  const auto* t = crc_table();
  std::uint32_t c = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < len; ++i) c = t[(c ^ data[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}

inline std::uint32_t adler32(const std::uint8_t* data, std::size_t len) {
  std::uint32_t a = 1, b = 0;
  for (std::size_t i = 0; i < len; ++i) {
    a = (a + data[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

inline void put_be32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 24));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v));
}

inline void append_chunk(std::vector<std::uint8_t>& out, const char* type,
                         const std::vector<std::uint8_t>& payload) {
  put_be32(out, static_cast<std::uint32_t>(payload.size()));
  const std::size_t typeStart = out.size();
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(type[i]));
  out.insert(out.end(), payload.begin(), payload.end());
  const std::uint32_t crc =
      crc32(out.data() + typeStart, payload.size() + 4);
  put_be32(out, crc);
}

}  // namespace detail

// 将 RGB24 帧缓冲编码为 PNG 字节流（确定性：无元数据、无时间戳）。
inline std::vector<std::uint8_t> encode_png(int width, int height,
                                            const std::vector<std::uint8_t>& rgb) {
  // 原始扫描流：每行前置过滤字节 0x00（None）。
  const std::size_t stride = static_cast<std::size_t>(width) * 3;
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(height) * (stride + 1));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0x00);
    raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(y) * static_cast<std::ptrdiff_t>(stride),
               rgb.begin() + static_cast<std::ptrdiff_t>((y + 1)) * static_cast<std::ptrdiff_t>(stride));
  }

  // zlib 流：0x78 0x01 头 + stored deflate 块（每块 ≤65535）+ Adler32。
  std::vector<std::uint8_t> zbuf;
  zbuf.push_back(0x78);
  zbuf.push_back(0x01);
  std::size_t off = 0;
  while (off < raw.size()) {
    const std::size_t block = (raw.size() - off < 65535) ? raw.size() - off : 65535;
    const bool last = (off + block == raw.size());
    zbuf.push_back(last ? 1 : 0);
    zbuf.push_back(static_cast<std::uint8_t>(block & 0xFF));
    zbuf.push_back(static_cast<std::uint8_t>(block >> 8));
    zbuf.push_back(static_cast<std::uint8_t>(~block & 0xFF));
    zbuf.push_back(static_cast<std::uint8_t>((~block >> 8) & 0xFF));
    zbuf.insert(zbuf.end(), raw.begin() + static_cast<std::ptrdiff_t>(off),
                raw.begin() + static_cast<std::ptrdiff_t>(off + block));
    off += block;
  }
  const std::uint32_t adler = detail::adler32(raw.data(), raw.size());
  detail::put_be32(zbuf, adler);

  std::vector<std::uint8_t> png{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<std::uint8_t> ihdr;
  detail::put_be32(ihdr, static_cast<std::uint32_t>(width));
  detail::put_be32(ihdr, static_cast<std::uint32_t>(height));
  ihdr.push_back(8);   // bit depth
  ihdr.push_back(2);   // color type: truecolor RGB
  ihdr.push_back(0);   // compression
  ihdr.push_back(0);   // filter
  ihdr.push_back(0);   // interlace
  detail::append_chunk(png, "IHDR", ihdr);
  detail::append_chunk(png, "IDAT", zbuf);
  detail::append_chunk(png, "IEND", {});
  return png;
}

// 原子写出：先建父目录，再写临时文件并 rename（契约：不留半成品）。
inline bool write_png_atomic(const std::string& path, int width, int height,
                             const std::vector<std::uint8_t>& rgb, std::string* err) {
  const std::vector<std::uint8_t> bytes = encode_png(width, height, rgb);
  std::error_code ec;
  const auto parent = std::filesystem::path(path).parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec && !std::filesystem::is_directory(parent)) {
      if (err) *err = "无法创建输出目录: " + parent.string() + " (" + ec.message() + ")";
      return false;
    }
  }
  const std::string tmp = path + ".tmp";
  std::FILE* f = std::fopen(tmp.c_str(), "wb");
  if (f == nullptr) {
    if (err) *err = "无法创建临时文件: " + tmp;
    return false;
  }
  const std::size_t written = std::fwrite(bytes.data(), 1, bytes.size(), f);
  std::fclose(f);
  if (written != bytes.size()) {
    std::remove(tmp.c_str());
    if (err) *err = "写入不完整: " + tmp;
    return false;
  }
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    std::remove(tmp.c_str());
    if (err) *err = "无法落盘到 " + path;
    return false;
  }
  return true;
}

}  // namespace mcruntime::plot
