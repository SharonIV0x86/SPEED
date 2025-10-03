#include "../include/AccessRegistry.hpp"

namespace SPEED {

AccessRegistry::AccessRegistry(const std::filesystem::path &ac_path,
                               const std::string &proc_name)
    : ac_path_(ac_path), access_filename_(proc_name) {
  if (!Utils::directoryExists(ac_path)) {
    Utils::createAccessRegistryDir(ac_path);
  }
  if (Utils::fileExists(ac_path_ / (proc_name + ".oregistry"))) {
    // throw std::runtime_error("[ERROR]: Access file already exists!");
    removeAccessFile();
  }
  this->proc_name_ = proc_name;
  putAccessFile();
  incrementalBuildGlobalRegistry();
  printRegistry();
}

const std::filesystem::path &AccessRegistry::getAccessRegistryPath() const {
  return ac_path_;
}

void AccessRegistry::incrementalBuildGlobalRegistry() {
  try {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(ac_path_)) {
      std::string name = entry.path().filename().string();
      if(name == proc_name_) continue;
      if (global_registry_.find(name) == global_registry_.end()) {
        global_registry_.insert(name);
        std::cout << "[INFO] Added to registry: " << name << "\n";
      } else {
        std::cout << "[SKIP] Already present: " << name << "\n";
      }
    }
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << "[ERROR] Filesystem error: " << e.what() << "\n";
  }
}

void AccessRegistry::printRegistry() const {
  std::cout << "=== Global Registry ===\n";
  for (const auto &item : global_registry_) {
    std::cout << item << "\n";
  }
}

void AccessRegistry::putAccessFile() {
  std::lock_guard<std::mutex> lock(mtx_);
  const std::filesystem::path before_path =
      ac_path_ / (proc_name_ + ".iregistry");
  const std::filesystem::path after_path =
      ac_path_ / (proc_name_ + ".oregistry");
  std::ofstream outstream(before_path);
  outstream << proc_name_ << "\n";
  outstream.close();
  std::rename(before_path.c_str(), after_path.c_str());
}
bool AccessRegistry::checkGlobalRegistry(const std::string &proc_name) const {
  return global_registry_.find(proc_name) != global_registry_.end();
}

bool AccessRegistry::checkAccess(const std::string &proc_name) const {
  return allowedProcesses_.find(proc_name) != allowedProcesses_.end();
}

void AccessRegistry::addProcessToList(const std::string &proc_name) {
  // std::cout << "Inside addProcessToList\n";
  std::lock_guard<std::mutex> lock(mtx_);

  // if (checkAccess(proc_name)) {
  //   std::cout << "[ERROR]: Process Already Exists in Access Registry\n";
  //   return;
  // }

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
  // std::cout << "[INFO]: Process Added to Allowed Registry\n";
}

bool AccessRegistry::removeProcessFromList(const std::string &proc_name) {
  std::lock_guard<std::mutex> lock(mtx_);

  auto it = allowedProcesses_.find(proc_name);
  if (it != allowedProcesses_.end()) {
    allowedProcesses_.erase(it);
    std::cout << "[INFO]: Process removed from Allowed Registry\n";
    return true;
  }
  std::cout << "[WARN]: Process not in Allowed Registry\n";
  return false;
}
void AccessRegistry::removeAccessFile() {
  std::lock_guard<std::mutex> lock(mtx_);
  const std::filesystem::path ac_removal_path_ =
      ac_path_ / (proc_name_ + ".oregistry");
  if (!Utils::fileExists(ac_removal_path_)) {
    std::cout
        << "[ERROR]: Unable to remove the access file, file doesnt exist\n";
  }
  std::filesystem::remove(ac_removal_path_);
}

} // namespace SPEED
