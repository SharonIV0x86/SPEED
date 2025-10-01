#include "../include/Utils.hpp"

namespace SPEED {
namespace Utils {
std::filesystem::path getDefaultSPEEDDir() {
  std::filesystem::path tmp_dir;

#ifdef _WIN32
  // On Windows, check TMP, TEMP, or LOCALAPPDATA
  if (const char *env_tmp = std::getenv("TMP")) {
    tmp_dir = env_tmp;
  } else if (const char *env_temp = std::getenv("TEMP")) {
    tmp_dir = env_temp;
  } else if (const char *env_local = std::getenv("LOCALAPPDATA")) {
    tmp_dir = env_local;
  } else {
    tmp_dir = "C:\\Windows\\Temp"; // fallback
  }
#else
  if (const char *env_tmpdir = std::getenv("TMPDIR")) {
    tmp_dir = env_tmpdir;
  } else {
    tmp_dir = "/tmp";
  }
#endif

  tmp_dir /= "speed";
  std::filesystem::create_directories(tmp_dir);

  return tmp_dir;
}

uint64_t getProcessID() {
#if defined(_WIN32)
  return static_cast<uint64_t>(GetCurrentProcessId());
#elif defined(__unix__) || defined(__APPLE__)
  return static_cast<uint64_t>(getpid());
#else
#error "Unsupported platform"
#endif
}
bool validateKey(const std::string &b64_key) {
  if (b64_key.empty())
    return false;

  unsigned char key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
  if (sodium_base642bin(key, sizeof(key), b64_key.c_str(), b64_key.size(),
                        nullptr, nullptr, nullptr,
                        sodium_base64_VARIANT_ORIGINAL) != 0) {
    std::cerr << "[ERROR]: Key is not valid Base64 or has wrong size\n";
    return false;
  }
  return true;
}

std::string getCurrentTimestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_time_t = system_clock::to_time_t(now);
  auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::ostringstream oss;
  oss << std::put_time(std::localtime(&now_time_t),
                       "%Y%m%d%H%M%S") // removed 'T'
      << std::setw(3) << std::setfill('0') << now_ms.count();
  return oss.str(); // e.g. "20251001225401123"
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
