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

bool SPEED::addProcess(const std::string &proc_name) {
  std::lock_guard<std::mutex> lock(access_list_mutex_);

  if (!access_list_->checkAccess(proc_name)) {
    std::cout << "[ERROR]: Process Already Exists in Access Registry\n";
    return false;
  }
  if (access_list_) {
    if (!access_list_->check_connection(proc_name)) {
      std::cout << "[SPECIAL] Running\n";
      access_list_->addProcessToList(proc_name);
    }
  }

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
  const std::unordered_set<std::string> acl = access_list_->getAccessList();
  std::vector<uint64_t> k(key_.begin(), key_.end());
  for (const std::string &entry : acl) {
    std::cout << "[DEBUG]: Broadcasting exit notif to: " << entry << "\n\n";
    Message exit_message = Message::construct_EXIT_NOTIF(entry);
    exit_message.header.seq_num = seq_number_;
    exit_message.header.sender = self_proc_name_;
    EncryptionManager::Encrypt(exit_message, k);
    BinaryManager::writeBinary(exit_message, speed_dir_, seq_number_, entry);

    seq_number_.fetch_add(1, std::memory_order_relaxed);
  }
}

void SPEED::sendMessage(const std::string &msg,
                        const std::string &reciever_name) {
  if (!access_list_->checkGlobalRegistry(reciever_name)) {
    std::cout << "[WARN] Process: " << reciever_name
              << " not in global registry list" << "\n";
    access_list_->incrementalBuildGlobalRegistry();
  }
  if (!access_list_->checkAccess(reciever_name)) {
    std::cout << "[WARN] Process: " << reciever_name << " not in access list"
              << "\n";
  }
  if (!access_list_->check_connection(reciever_name)) {
    std::cout << "[WARN] Process: " << reciever_name
              << " not in connection list" << "\n";
  }
  Message message = Message::construct_MSG(msg);
  message.header.seq_num = seq_number_;
  message.header.sender = self_proc_name_;
  message.header.reciever = reciever_name;
  std::vector<uint64_t> k(key_.begin(), key_.end());
  if (!Message::validate_message_sent(message, self_proc_name_,
                                      reciever_name)) {
    std::cout << "[ERROR] Message validation failed! Before." << "\n";
    Message::print_message(message);
  }
  EncryptionManager::Encrypt(message, k);
  BinaryManager::writeBinary(message, speed_dir_, seq_number_, reciever_name);

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
  if (!Message::validate_message_recieved(msg, self_proc_name_)) {
    std::cout << "[ERROR]: Invalid Message recieved! Not Processing.\n";
    Message::print_message(msg);
    return;
  }
  switch (msg.header.type) {
  case MessageType::MSG: {
    PMessage mm = Message::destruct_message(msg);
    callback_(mm);
    break;
  }
  case MessageType::EXIT_NOTIF: {
    const std::string s(msg.payload.begin(), msg.payload.end());
    std::cout << "[DEBUG]: Recieved and EXIT_NOTIF for: " << s << "\n";
    access_list_->removeProcessFromGlobalRegistry(msg.header.sender);
    access_list_->removeProcessFromAccessList(msg.header.sender);
    access_list_->removeProcessFromConnectedList(msg.header.sender);
    break;
  }
  case MessageType::CON_REQ: {
    std::cout << "[INFO]: Recieved CON_REQ from: " << msg.header.sender << "\n";
    break;
  }
  case MessageType::PING: {
    std::cout << "[INFO]: Recieved PING from: " << msg.header.sender << "\n";
    std::cout << "[INFO]: Now trying to send a pong message\n";
    pong(msg.header.sender);
    break;
  }
  case MessageType::PONG: {
    PMessage mm = Message::destruct_message(msg);
    callback_(mm);
    break;
  }
  }
  std::filesystem::remove(file_path, ec);
  {
    std::lock_guard<std::mutex> lock(seen_mutex_);
    seen_.erase(file_path.filename().string());
  }
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
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}
void SPEED::ping(const std::string &reciever_name) { ping_(reciever_name); }
void SPEED::pong(const std::string &reciever_name) { pong_(reciever_name); }
void SPEED::ping_(const std::string &reciever_name) {
  Message ping_message = Message::construct_PING(reciever_name);
  ping_message.header.seq_num = seq_number_;
  ping_message.header.sender = self_proc_name_;
  std::vector<uint64_t> k(key_.begin(), key_.end());
  EncryptionManager::Encrypt(ping_message, k);
  BinaryManager::writeBinary(ping_message, speed_dir_, seq_number_,
                             reciever_name);
  seq_number_.fetch_add(1, std::memory_order_relaxed);
}
void SPEED::pong_(const std::string &reciever_name) {
  Message pong_message = Message::construct_PONG(reciever_name);
  pong_message.header.seq_num = seq_number_;
  pong_message.header.sender = self_proc_name_;
  std::cout << "\n[INFO]: Sending a PONG to: " << reciever_name << "\n";
  std::vector<uint64_t> k(key_.begin(), key_.end());
  EncryptionManager::Encrypt(pong_message, k);
  BinaryManager::writeBinary(pong_message, speed_dir_, seq_number_,
                             reciever_name);
  seq_number_.fetch_add(1, std::memory_order_relaxed);
}
} // namespace SPEED
