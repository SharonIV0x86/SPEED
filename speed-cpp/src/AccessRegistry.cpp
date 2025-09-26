#include "../include/AccessRegistry.hpp"

namespace SPEED {

AccessRegistry::AccessRegistry(const std::filesystem::path &ac_path)
    : ac_path_(ac_path) {}

const std::filesystem::path &AccessRegistry::getAccessRegistryPath() const {
  return ac_path_;
}

void AccessRegistry::incrementalBuildGlobalRegistry() {
  // Mock incremental build (could scan files)
  global_registry_.insert("Jasper");
  global_registry_.insert("Tobias");
  global_registry_.insert("Lestrade");
}

bool AccessRegistry::checkGlobalRegistry(const std::string &proc_name) const {
  return global_registry_.find(proc_name) != global_registry_.end();
}

bool AccessRegistry::checkAccess(const std::string &proc_name) const {
  return allowedProcesses_.find(proc_name) != allowedProcesses_.end();
}

void AccessRegistry::addProcessToList(const std::string &proc_name) {
  // std::cout << "Inside addProcessToList\n";

  if (checkAccess(proc_name)) {
    std::cout << "[ERROR]: Process Already Exists in Access Registry\n";
    return;
  }

  if (!checkGlobalRegistry(proc_name)) {
    std::cout << "[INFO]: Process not in global registry, rebuilding...\n";
    incrementalBuildGlobalRegistry();
    if (!checkGlobalRegistry(proc_name)) {
      std::cout << "[ERROR]: Process cannot be added, not in global registry\n";
      return;
    }
  }

  // Safe add
  allowedProcesses_.insert(proc_name);
  std::cout << "[INFO]: Process Added to Allowed Registry\n";
}

bool AccessRegistry::removeProcessFromList(const std::string &proc_name) {
  auto it = allowedProcesses_.find(proc_name);
  if (it != allowedProcesses_.end()) {
    allowedProcesses_.erase(it);
    std::cout << "[INFO]: Process removed from Allowed Registry\n";
    return true;
  }
  std::cout << "[WARN]: Process not in Allowed Registry\n";
  return false;
}

} // namespace SPEED
