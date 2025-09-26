#include "../include/Utils.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace SPEED {
namespace Utils {

std::filesystem::path getDefaultSPEEDDir() {
  std::filesystem::path base_dir;
#ifdef _WIN32
  base_dir = std::getenv("LOCALAPPDATA");
#else
  if (const char *xdg_data_home = std::getenv("XDG_DATA_HOME")) {
    base_dir = xdg_data_home;
  } else {
    base_dir = std::getenv("HOME");
    base_dir /= ".local/share";
  }
#endif
  base_dir /= "speed"; // <-- typo? maybe meant "speed"
  return base_dir;
}

std::string getCurrentTimestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_time_t = system_clock::to_time_t(now);
  auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::ostringstream oss;
  oss << std::put_time(std::localtime(&now_time_t), "%Y%m%dT%H%M%S")
      << std::setw(3) << std::setfill('0') << now_ms.count();
  return oss.str();
}

std::string generateUUID() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

  uint32_t data[4];
  for (auto &d : data)
    d = dist(gen);

  // Set version and variant bits
  data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
  data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(8) << data[0] << "-"
      << std::setw(4) << (data[1] >> 16) << "-" << std::setw(4)
      << (data[1] & 0xFFFF) << "-" << std::setw(4) << (data[2] >> 16) << "-"
      << std::setw(4) << (data[2] & 0xFFFF) << std::setw(8) << data[3];

  return oss.str();
}

std::string getTimestampUUID() {
  return getCurrentTimestamp() + "_" + generateUUID();
}
bool createDefaultDir(const std::filesystem::path &base_dir) {
  try {
    if (!std::filesystem::exists(base_dir)) {
      if (std::filesystem::create_directories(base_dir)) {
        return true;
      } else {
        return false;
      }
    } else {
      std::cout << "Directory already exists: " << base_dir << "\n";
    }
  } catch (const std::exception &e) {
    std::cerr << "Error creating directory: " << e.what() << "\n";
  }
  return false;
}
bool createAccessRegistryDir(const std::filesystem::path &ac_dir) {
  try {
    if (!std::filesystem::exists(ac_dir)) {
      if (std::filesystem::create_directories(ac_dir)) {
        return true;
      } else {
        return false;
      }
    } else {
      std::cout << "Directory already exists: " << ac_dir << "\n";
    }
  } catch (const std::exception &e) {
    std::cerr << "Error creating directory: " << e.what() << "\n";
  }
  return false;
}
} // namespace Utils
} // namespace SPEED
