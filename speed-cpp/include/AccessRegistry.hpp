#pragma once
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>

namespace SPEED {

class AccessRegistry {
public:
  AccessRegistry(const std::filesystem::path &path);

  void addProcessToList(const std::string &proc_name);
  bool removeProcessFromList(const std::string &proc_name);
  bool checkAccess(const std::string &proc_name) const;
  bool checkGlobalRegistry(const std::string &proc_name) const;
  void incrementalBuildGlobalRegistry();
  const std::filesystem::path &getAccessRegistryPath() const;

private:
  std::unordered_set<std::string> allowedProcesses_; // subscribed processes
  std::unordered_set<std::string> global_registry_;  // all known processes
  std::filesystem::path ac_path_;
};

} // namespace SPEED
