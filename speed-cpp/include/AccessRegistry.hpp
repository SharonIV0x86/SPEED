#pragma once
#include "Utils.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>
namespace SPEED {
class AccessRegistry {
public:
  AccessRegistry(const std::filesystem::path &, const std::string &);

  void addProcessToList(const std::string &proc_name);
  bool removeProcessFromList(const std::string &proc_name);
  void removeAccessFile();
  const std::filesystem::path &getAccessRegistryPath() const;
  bool checkAccess(const std::string &proc_name) const;

private:
  bool checkGlobalRegistry(const std::string &proc_name) const;
  void putAccessFile();
  void incrementalBuildGlobalRegistry();
  void printRegistry() const;
  std::unordered_set<std::string> allowedProcesses_;
  std::unordered_set<std::string> global_registry_;

  std::filesystem::path ac_path_;
  std::string access_filename_;
  std::string proc_name_;

  mutable std::mutex mtx_; // protects shared state
};

} // namespace SPEED
