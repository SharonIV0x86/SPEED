#pragma once
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sodium.h>
#include <sstream>
#include <string>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif
namespace SPEED {
namespace Utils {

std::filesystem::path getDefaultSPEEDDir();
std::string getCurrentTimestamp();
std::string generateUUID();
std::string getTimestampUUID();
bool createDefaultDir(const std::filesystem::path &);
bool createAccessRegistryDir(const std::filesystem::path &);
uint64_t getProcessID();
bool validateKey(const std::string &);
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
