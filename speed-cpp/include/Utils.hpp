#pragma once
#include <filesystem>
#include <string>

namespace SPEED {
namespace Utils {

// Functions to be implemented in Utils.cpp
std::filesystem::path getDefaultSPEEDDir();
std::string getCurrentTimestamp();
std::string generateUUID();
std::string getTimestampUUID();
bool createDefaultDir(const std::filesystem::path &);
bool createAccessRegistryDir(const std::filesystem::path &);
// These can stay inline (safe, header-only)
inline bool fileExists(const std::filesystem::path &fpath) {
  try {
    return std::filesystem::exists(fpath) &&
           std::filesystem::is_regular_file(fpath);
  } catch (...) {
    return false;
  }
}

inline bool directoryExists(const std::filesystem::path &fpath) {
  try {
    return std::filesystem::exists(fpath) &&
           std::filesystem::is_directory(fpath);
  } catch (...) {
    return false;
  }
}

} // namespace Utils
} // namespace SPEED
