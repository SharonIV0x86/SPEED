#include "../include/KeyManager.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

std::string
SPEED::KeyManager::getKeyFromConfigFile(const std::filesystem::path &fpath) {
  if (!std::filesystem::exists(fpath)) {
    throw std::runtime_error("Config file does not exist: " + fpath.string());
  }

  std::ifstream infile(fpath);
  if (!infile) {
    throw std::runtime_error("Failed to open config file: " + fpath.string());
  }

  std::string contents((std::istreambuf_iterator<char>(infile)),
                       std::istreambuf_iterator<char>());

  while (!contents.empty() &&
         (contents.back() == '\n' || contents.back() == '\r' ||
          contents.back() == ' ')) {
    contents.pop_back();
  }

  return contents;
}
