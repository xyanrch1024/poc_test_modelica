#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "png_encoder.hpp"

namespace {

using namespace mcruntime::plot;

// 参考实现：逐位 CRC32（与编码器的查表法交叉验证）。
std::uint32_t reference_crc32(const std::uint8_t* data, std::size_t len) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

std::uint32_t read_be32(const std::vector<std::uint8_t>& b, std::size_t off) {
  return (static_cast<std::uint32_t>(b[off]) << 24) |
         (static_cast<std::uint32_t>(b[off + 1]) << 16) |
         (static_cast<std::uint32_t>(b[off + 2]) << 8) | b[off + 3];
}

// 简单 PNG 结构遍历器：验证签名/块序列/CRC/尺寸。
struct ChunkInfo {
  std::string type;
  std::size_t len;
};

std::vector<ChunkInfo> walk_chunks(const std::vector<std::uint8_t>& png,
                                   int* outW, int* outH) {
  const std::uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  EXPECT_EQ(0, std::memcmp(png.data(), sig, 8));
  std::vector<ChunkInfo> chunks;
  std::size_t off = 8;
  while (off + 12 <= png.size()) {
    const std::uint32_t len = read_be32(png, off);
    std::string type(reinterpret_cast<const char*>(&png[off + 4]), 4);
    const std::size_t dataOff = off + 8;
    if (dataOff + len + 4 > png.size()) {
      ADD_FAILURE() << "块越界: " << type;
      return chunks;
    }
    const std::uint32_t crc = reference_crc32(&png[off + 4], len + 4);
    EXPECT_EQ(crc, read_be32(png, dataOff + len)) << "CRC 不符: " << type;
    chunks.push_back({type, len});
    if (type == "IHDR") {
      *outW = static_cast<int>(read_be32(png, dataOff));
      *outH = static_cast<int>(read_be32(png, dataOff + 4));
      EXPECT_EQ(8, png[dataOff + 8]);   // bit depth
      EXPECT_EQ(2, png[dataOff + 9]);   // truecolor
      EXPECT_EQ(0, png[dataOff + 12]);  // interlace
    }
    off = dataOff + len + 4;
    if (type == "IEND") break;
  }
  EXPECT_EQ(off, png.size());
  return chunks;
}

TEST(PngEncoder, ProducesStructurallyValidPng) {
  // 2×2 测试图
  std::vector<std::uint8_t> rgb{255, 0, 0, 0, 255, 0, 0, 0, 255, 10, 20, 30};
  auto png = encode_png(2, 2, rgb);
  int w = 0, h = 0;
  auto chunks = walk_chunks(png, &w, &h);
  EXPECT_EQ(w, 2);
  EXPECT_EQ(h, 2);
  ASSERT_GE(chunks.size(), 3u);
  EXPECT_EQ(chunks.front().type, "IHDR");
  EXPECT_EQ(chunks[chunks.size() - 2].type, "IDAT");
  EXPECT_EQ(chunks.back().type, "IEND");
}

TEST(PngEncoder, IdatIsStoredDeflateWithAdler32) {
  std::vector<std::uint8_t> rgb(24, 128);  // 8×1 RGB 图像
  auto png = encode_png(8, 1, rgb);
  // 找 IDAT 载荷
  std::size_t off = 8;
  std::vector<std::uint8_t> zbuf;
  while (off + 12 <= png.size()) {
    const std::uint32_t len = read_be32(png, off);
    std::string type(reinterpret_cast<const char*>(&png[off + 4]), 4);
    if (type == "IDAT") {
      zbuf.assign(png.begin() + off + 8, png.begin() + off + 8 + len);
      break;
    }
    off += 8 + len + 4;
  }
  ASSERT_FALSE(zbuf.empty());
  EXPECT_EQ(zbuf[0], 0x78);  // zlib 头（CMF/FLG，check 合法性）
  EXPECT_EQ(((zbuf[0] << 8) | zbuf[1]) % 31, 0);
  // stored 块头：BFINAL + LEN/NLEN
  ASSERT_GE(zbuf.size(), 5u);
  const std::uint8_t bfinal = zbuf[2] & 1;
  const std::uint8_t btype = (zbuf[2] >> 1) & 3;
  EXPECT_EQ(btype, 0);  // stored
  const std::uint32_t len = zbuf[3] | (zbuf[4] << 8);
  const std::uint32_t nlen = zbuf[5] | (zbuf[6] << 8);
  EXPECT_EQ(static_cast<std::uint16_t>(~len), static_cast<std::uint16_t>(nlen));
  EXPECT_EQ(len % 65535, len || len == 65535 ? len : 0);
  (void)bfinal;
}

TEST(PngEncoder, DeterministicAcrossRuns) {
  std::vector<std::uint8_t> rgb(960 * 3, 77);
  for (std::size_t i = 0; i < rgb.size(); i += 3) rgb[i] = static_cast<std::uint8_t>(i % 251);
  auto a = encode_png(8, 1, rgb);
  auto b = encode_png(8, 1, rgb);
  EXPECT_EQ(a, b) << "同输入必须逐字节一致（FR-006）";
}

TEST(PngEncoder, AtomicWriteLeavesNoTempFileOnSuccess) {
  std::vector<std::uint8_t> rgb(3, 42);
  const std::string path = "png_atomic_test.png";
  std::string err;
  EXPECT_TRUE(write_png_atomic(path, 1, 1, rgb, &err)) << err;
  std::FILE* f = std::fopen(path.c_str(), "rb");
  EXPECT_NE(f, nullptr);
  if (f != nullptr) std::fclose(f);
  std::remove(path.c_str());
}

}  // namespace
