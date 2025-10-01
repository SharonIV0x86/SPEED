#include "SPEED.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

namespace SPEED {

SPEED::SPEED(const std::string &proc_name, const ThreadMode &tmode,
             const std::filesystem::path &speed_dir) {
  self_proc_name_ = proc_name;
  tmode_ = tmode;
  speed_dir_ = speed_dir;
  self_speed_dir_ = speed_dir_ / proc_name;

  if (!Utils::directoryExists(speed_dir_)) {
    Utils::createDefaultDir(speed_dir_);
  }

  if (!Utils::directoryExists(self_speed_dir_)) {
    Utils::createDefaultDir(self_speed_dir_);
  }

  if (!Utils::directoryExists(speed_dir_ / "access_registry")) {
    Utils::createAccessRegistryDir(speed_dir_ / "access_registry");
  }
  // std::cout << "[INFO Speed Dir: " << speed_dir_ << "\n";
  access_list_ = std::make_unique<AccessRegistry>(
      speed_dir_ / "access_registry", proc_name);
}

SPEED::SPEED(const std::string &proc_name, const ThreadMode &tmode)
    : SPEED(proc_name, tmode, Utils::getDefaultSPEEDDir()) {}

SPEED::~SPEED() { kill(); }

bool SPEED::setKeyFile(const std::filesystem::path &key_path) {
  if (!Utils::fileExists(key_path))
    return false;
  std::lock_guard<std::mutex> lock(key_mutex_);
  key_ = KeyManager::getKeyFromConfigFile(key_path);
  if (!Utils::validateKey(key_)) {
    std::cout << "[ERROR]: Invalid Key\n";
    throw std::runtime_error("Invalid Key\n");
  }
  // std::cout << "[INFO]: Retrieved Key: " << key_ << "\n";
  key_path_ = key_path;
  return true;
}

void SPEED::setCallback(std::function<void(const PMessage &)> cb) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  callback_ = std::move(cb);
}

// void SPEED::trigger() {
//   std::function<void(const PMessage &)> cb_copy;
//   {
//     std::lock_guard<std::mutex> lock(callback_mutex_);
//     cb_copy = callback_;
//   }
//   if (cb_copy) {
//     PMessage msg("Lemon", "BING BONG", 69420);
//     cb_copy(msg);
//   }
// }

bool SPEED::addProcess(const std::string &proc_name) {
  std::lock_guard<std::mutex> lock(access_list_mutex_);

  if (access_list_->checkAccess(proc_name)) {
    return false;
  }

  if (access_list_)
    access_list_->addProcessToList(proc_name);

  return true;
}

void SPEED::start() {
  watcher_should_exit_.store(false);

  if (watcher_running_.load())
    return;

  watcher_running_.store(true);

  if (tmode_ == ThreadMode::Single) {
    // In single-thread mode, run watcher in main loop (blocking for
    // bare-metal/embedded)
    watcherSingleThread_();
  } else {
    // Multi-thread mode: watcher runs in background thread
    watcher_thread_ = std::thread([this]() { watcherMultiThread_(); });
    watcher_thread_.detach();
  }
}

void SPEED::stop() { watcher_should_exit_.store(true); }

void SPEED::resume() {
  if (tmode_ == ThreadMode::Multi && !watcher_running_.load()) {
    start();
  }
}

void SPEED::kill() {
  watcher_should_exit_.store(true);
  watcher_running_.store(false);
  access_list_->removeAccessFile();
}

void SPEED::sendMessage(const std::string &msg, const std::string &rec_name) {
  Message message;
  message.header.version = SPEED_VERSION;
  message.header.type = MessageType::MSG;
  message.header.sender_pid = Utils::getProcessID();
  message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
  message.header.seq_num = seq_number_;
  message.header.sender = self_proc_name_;
  message.header.reciever = rec_name;

  message.payload = std::vector<uint8_t>(msg.begin(), msg.end());
  std::vector<uint64_t> k(key_.begin(), key_.end());
  EncryptionManager::Encrypt(message, k);
  BinaryManager::writeBinary(message, speed_dir_, seq_number_, rec_name);

  seq_number_.fetch_add(1, std::memory_order_relaxed);
}

std::optional<long long>
SPEED::extractSeqFromFilename_(const std::string &filename) const {
  static const std::regex re("^(\\d+)_.*\\.ospeed$");
  std::smatch m;
  if (std::regex_search(filename, m, re) && m.size() >= 2) {
    try {
      return std::stoll(m[1].str());
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

void SPEED::processFile_(const std::filesystem::path &file_path) {
  std::error_code ec;
  std::lock_guard<std::mutex> lock(callback_mutex_);
  Message msg = BinaryManager::readBinary(file_path);
  std::vector<uint64_t> k(key_.begin(), key_.end());
  EncryptionManager::Decrypt(msg, k);
  std::string version = std::to_string(msg.header.version);
  MessageType type = msg.header.type;
  std::string sender_pid = std::to_string(msg.header.sender_pid);
  std::string timestamp = std::to_string(msg.header.timestamp);
  std::string seq_num = std::to_string(msg.header.seq_num);
  std::string sender = msg.header.sender;
  std::string reciever = msg.header.reciever;
  std::string payload = std::string(msg.payload.begin(), msg.payload.end());

  PMessage mm(sender, payload, timestamp);
  callback_(mm);
  // std::cout << "\n\n[INFO]: Got file: " << file_path << "\n";
  // std::cout << "Read Object Version: " << version << "\n";
  // std::cout << "Read Object Type: " << (int)type << "\n";
  // std::cout << "Read Object sender_pid: " << sender_pid << "\n";
  // std::cout << "Read Object timestamp: " << timestamp << "\n";
  // std::cout << "Read Object seq_num: " << seq_num << "\n";
  // std::cout << "Read Object sender: " << sender << "\n";
  // std::cout << "Read Object reciever: " << reciever << "\n";
  // std::cout << "Read Object Payload:" << payload << "\n";
  std::filesystem::remove(file_path, ec);
  {
    std::lock_guard<std::mutex> lock(seen_mutex_);
    seen_.erase(file_path.filename().string());
  }

  // also call the callback from here as well.
}

void SPEED::watcherSingleThread_() {
  std::lock_guard<std::mutex> lock(single_mtx_);
  // std::cout << "[INFO]: Starting Single-Thread Blocking Watcher\n";

  while (!watcher_should_exit_.load()) {
    for (auto &entry : std::filesystem::directory_iterator(self_speed_dir_)) {
      if (!entry.is_regular_file())
        continue;

      std::string fname = entry.path().filename().string();

      if (seen_.count(fname))
        continue;

      auto seq = extractSeqFromFilename_(fname);
      if (seq) {
        heap_.emplace(*seq, entry.path());
        seen_.insert(fname);
      }
    }
    if (!heap_.empty()) {
      auto candidate = heap_.top();
      heap_.pop();
      seen_.erase(candidate.second.filename().string());

      processFile_(candidate.second);
      continue;
    }

    // Nothing to do → sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}
void SPEED::watcherMultiThread_() {
  std::lock_guard<std::mutex> lock(multi_mutex_);
  // std::cout << "[INFO]: Starting Multi-Thread Non-Blocking Watcher\n";
  while (!watcher_should_exit_.load()) {
    for (auto &entry : std::filesystem::directory_iterator(self_speed_dir_)) {
      if (!entry.is_regular_file())
        continue;

      auto fname = entry.path().filename().string();
      if (seen_.count(fname))
        continue;

      auto seq = extractSeqFromFilename_(fname);
      if (seq) {
        heap_.emplace(*seq, entry.path());
        seen_.insert(fname);
      }
    }
    if (!heap_.empty()) {
      auto [seq, path] = heap_.top();
      heap_.pop();

      processFile_(path);
      continue;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

} // namespace SPEED
