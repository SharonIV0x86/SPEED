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
  std::cout << "[INFO]: Retrieved Key: " << key_ << "\n";
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
    cb_copy = callback_;
  }
  if (cb_copy) {
    PMessage msg("Lemon", "BING BONG", 69420);
    cb_copy(msg);
  }
}

void SPEED::addProcess(const std::string &proc_name) {
  std::lock_guard<std::mutex> lock(access_list_mutex_);
  if (access_list_)
    access_list_->addProcessToList(proc_name);
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
}

void SPEED::sendMessage() {
  // this is just for test here
  seq_number_.fetch_add(1, std::memory_order_relaxed);
}

std::optional<long long>
SPEED::extractSeqFromFilename_(const std::string &filename) const {
  static const std::regex re("(\\d+)\\.ospeed$");
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
  std::cout << "[INFO]: Got file: " << file_path << "\n";
  // std::ifstream ifs(file_path, std::ios::binary);
  // if (!ifs)
  //   return;
  // std::ostringstream ss;
  // ss << ifs.rdbuf();
  // Message msg;
  // processIncomingMessage_(msg);
  std::error_code ec;
  std::filesystem::remove(file_path, ec);
}

void SPEED::watcherSingleThread_() {
  std::lock_guard<std::mutex> lock(single_mtx_);
  std::cout << "[INFO]: Starting Single-Thread Blocking Watcher\n";

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
  std::cout << "[INFO]: Starting Multi-Thread Non-Blocking Watcher\n";
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

void SPEED::processIncomingMessage_(const Message &message) { (void)message; }
void SPEED::mockWrite() {
  std::lock_guard<std::mutex> lock(write_mutex_);
  Message msg;
  const std::string m = "I love biryani (veg)";
  msg.header.version = SPEED_VERSION;
  msg.header.type = MessageType::MSG;
  msg.header.sender_pid = 192;
  msg.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
  msg.header.seq_num = seq_number_;
  msg.header.sender = self_proc_name_;
  msg.header.reciever = "Murphys";
  msg.payload = std::vector<uint8_t>(m.begin(), m.end());
  bool status = BinaryManager::writeBinary(msg, speed_dir_, seq_number_);
  if (status)
    seq_number_.fetch_add(1);
}
Message SPEED::mockRead() {
  std::filesystem::path pth = speed_dir_ / "0.ospeed";
  return BinaryManager::readBinary(pth);
}
} // namespace SPEED
