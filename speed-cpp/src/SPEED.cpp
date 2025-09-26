#include "../include/SPEED.hpp"
#include <iostream>

namespace SPEED {

SPEED::SPEED(const std::string &proc_name, const ThreadMode &tmode,
             const std::filesystem::path &speed_dir) {
  this->self_proc_name_ = proc_name;
  this->tmode_ = tmode;
  this->speed_dir_ = speed_dir;

  if (!Utils::directoryExists(speed_dir_)) {
    std::cout << "[ERROR]: Speed Dir does not exist\n";
    if (Utils::createDefaultDir(speed_dir_)) {
      std::cout << "[INFO] Created Speed Dir\n";
    }
    return;
  }

  std::cout << "[INFO]: Speed DIR: " << speed_dir_ << "\n";
  if (!Utils::directoryExists(speed_dir_ / "access_registry")) {
    std::cout << "[ERROR]: Registry dir doesnt exist \n";
    if (Utils::createAccessRegistryDir(speed_dir_ / "access_registry")) {
      std::cout << "[INFO]: Created Registry Dir\n";
    }
  }

  access_list_ =
      std::make_unique<AccessRegistry>(speed_dir_ / "access_registry");
  std::cout << "[INFO]: Registry Dir: " << access_list_->getAccessRegistryPath()
            << "\n";
}

SPEED::SPEED(const std::string &proc_name, const ThreadMode &tmode)
    : SPEED(proc_name, tmode, Utils::getDefaultSPEEDDir()) {}

bool SPEED::setKeyFile(const std::filesystem::path &key_path) {
  if (!Utils::fileExists(key_path)) {
    std::cout << "[ERROR]: Key Path Doen't exist\n";
    return false;
  }
  key_ = KeyManager::getKeyFromConfigFile(key_path);
  this->key_path_ = key_path;
  return true;
}
void SPEED::setCallback(std::function<void(const PMessage &)> cb) {
  callback_ = std::move(cb);
}
void SPEED::processIncomingMessage_(const Message &message) {
  // validation and all is handled here.
}
void SPEED::addProcess(const std::string &proc_name) {
  access_list_->addProcessToList(proc_name);
}
void SPEED::trigger() {}
} // namespace SPEED
