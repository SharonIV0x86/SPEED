#include "../include/SPEED.hpp"
#include <iostream>

namespace SPEED {

SPEED::SPEED(const std::string &proc_name, const ThreadMode &tmode,
             const std::filesystem::path &speed_dir) {
  self_proc_name_ = proc_name;
  tmode_ = tmode;
  speed_dir_ = speed_dir;

  if (!Utils::directoryExists(speed_dir_)) {
    std::cout << "[ERROR]: Speed Dir does not exist\n";
    if (Utils::createDefaultDir(speed_dir_)) {
      std::cout << "[INFO] Created Speed Dir\n";
    }
    return;
  }

  std::cout << "[INFO]: Speed DIR: " << speed_dir_ << "\n";
  if (!Utils::directoryExists(speed_dir_ / "access_registry")) {
    std::cout << "[ERROR]: Registry dir doesn't exist \n";
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
    std::cout << "[ERROR]: Key Path doesn't exist\n";
    return false;
  }

  std::lock_guard<std::mutex> lock(key_mutex_);
  key_ = KeyManager::getKeyFromConfigFile(key_path);
  key_path_ = key_path;
  return true;
}

void SPEED::setCallback(std::function<void(const PMessage &)> cb) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  callback_ = std::move(cb);
}

void SPEED::trigger() {
  std::function<void(const PMessage &)> cb_copy;
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    cb_copy = callback_; // copy the callback to call outside the lock
  }

  if (cb_copy) {
    PMessage msg("Lemon", "Hello lol", 69420);
    cb_copy(msg);
  }
}

void SPEED::addProcess(const std::string &proc_name) {
  std::lock_guard<std::mutex> lock(access_list_mutex_);
  if (access_list_) {
    access_list_->addProcessToList(proc_name);
  }
}

void SPEED::processIncomingMessage_(const Message &message) {
  // validation and handling here
  // acquire locks if modifying callback_, access_list_, key_, etc.
}

} // namespace SPEED
