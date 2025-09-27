#pragma once
#include "BinaryMessage.hpp" // your Message and MessageHeader definitions
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
                          std::atomic<long long> &);
  static Message readBinary(const std::filesystem::path &);
};

template <typename T> std::array<uint8_t, sizeof(T)> to_big_endian(T value) {
  static_assert(std::is_integral_v<T>, "Integral type required");
  std::array<uint8_t, sizeof(T)> out{};
  for (size_t i = 0; i < sizeof(T); ++i) {
    out[sizeof(T) - 1 - i] = static_cast<uint8_t>(value & 0xFF);
    value >>= 8;
  }
  return out;
}

// Convert big-endian byte array back to integral
template <typename T> T inline from_big_endian(const uint8_t *data) {
  static_assert(std::is_integral_v<T>, "Integral type required");
  T value = 0;
  for (size_t i = 0; i < sizeof(T); ++i) {
    value = (value << 8) | data[i];
  }
  return value;
}
template <typename T>
static void inline write_uint(std::ofstream &out, T value) {
  auto bytes = to_big_endian(value);
  out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
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
