#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
namespace SPEED {
class KeyManager {
public:
  static std::string getKeyFromConfigFile(const std::filesystem::path &);
};
} // namespace SPEED
