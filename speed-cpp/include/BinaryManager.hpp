#pragma once
#include "BinaryMessage.hpp"
#include "Utils.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
namespace SPEED {

class BinaryManager {
public:
  static bool writeBinary(const Message &, const std::filesystem::path &,
                          std::atomic<long long> &, const std::string &);
  static Message readBinary(const std::filesystem::path &);
};

template <typename T> std::vector<unsigned char> to_big_endian(T value) {
  std::vector<unsigned char> bytes(sizeof(T));
  for (size_t i = 0; i < sizeof(T); ++i) {
    if constexpr (sizeof(T) > 1)
      bytes[sizeof(T) - 1 - i] = (value >> (i * 8)) & 0xFF;
    else
      bytes[0] = value; // no shift for 1-byte types
  }
  return bytes;
}

// Convert big-endian byte array back to integral
template <typename T> T from_big_endian(const uint8_t *buf) {
  static_assert(std::is_integral_v<T>,
                "from_big_endian requires integral type");
  T value = 0;
  for (size_t i = 0; i < sizeof(T); ++i) {
    value = (value << 8) | buf[i];
  }
  return value;
}

template <typename T> void write_uint(std::ofstream &out, T value) {
  static_assert(std::is_integral_v<T>, "write_uint requires integral type");
  unsigned char buf[sizeof(T)];
  for (size_t i = 0; i < sizeof(T); ++i) {
    buf[sizeof(T) - 1 - i] =
        static_cast<unsigned char>((value >> (i * 8)) & 0xFF);
  }
  out.write(reinterpret_cast<const char *>(buf), sizeof(T));
}

// template for reading integral types from big-endian
template <typename T> static inline T read_uint(std::ifstream &in) {
  uint8_t buf[sizeof(T)];
  in.read(reinterpret_cast<char *>(buf), sizeof(T));
  return from_big_endian<T>(buf);
}

// strings
inline static void write_string(std::ofstream &out, const std::string &s) {
  write_uint(out, static_cast<uint32_t>(s.size()));
  out.write(s.data(), s.size());
}

inline static std::string read_string(std::ifstream &in) {
  uint32_t len = read_uint<uint32_t>(in);
  std::string s(len, '\0');
  in.read(&s[0], len);
  return s;
}

} // namespace SPEED
