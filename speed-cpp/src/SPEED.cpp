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
  if (!Utils::fileExists(key_path)) {
    std::cout << "[ERROR]: Key File Doesn't Exist. Exitting...\n";
    exit(EXIT_FAILURE);
    return false;
  }
  std::lock_guard<std::mutex> lock(key_mutex_);
  key_ = KeyManager::getKeyFromConfigFile(key_path);
  if (!Utils::validateKey(key_)) {
    std::cout << "[ERROR]: Invalid Key\n";
    throw std::runtime_error("Invalid Key\n");
  }
  key_path_ = key_path;
  return true;
}

void SPEED::setCallback(std::function<void(const PMessage &)> cb) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  callback_ = std::move(cb);
}

bool SPEED::addProcess(const std::string &proc_name) {
  std::lock_guard<std::mutex> lock(access_list_mutex_);

  if (access_list_) {
    if (!access_list_->check_connection(proc_name)) {
      access_list_->addProcessToList(proc_name);
      Message message = MessageUtils::construct_CON_REQ(proc_name);
      message.header.seq_num = seq_number_.load(std::memory_order_relaxed);
      message.header.sender = self_proc_name_;
      message.header.reciever = proc_name;
      std::vector<uint64_t> k(key_.begin(), key_.end());
      if (!MessageUtils::validate_message_sent(message, self_proc_name_,
                                               proc_name)) {
        std::cout << "[ERROR]: Messsage validation! Before. \n";
        MessageUtils::print_message(message);
        return false;
      }
      EncryptionManager::Encrypt(message, k);
      BinaryManager::writeBinary(message, speed_dir_, seq_number_, proc_name);
      con_res_buffer.insert(proc_name);
      seq_number_.fetch_add(1, std::memory_order_relaxed);
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
  stopAllExecutors();
  const std::unordered_set<std::string> acl = access_list_->getAccessList();
  std::vector<uint64_t> k(key_.begin(), key_.end());
  for (const std::string &entry : acl) {
    std::cout << "[DEBUG]: Broadcasting exit notif to: " << entry << "\n\n";
    Message exit_message = MessageUtils::construct_EXIT_NOTIF(entry);
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
    std::cout << "[ERROR]: Cannot send Message to: " << reciever_name
              << " as it is not in Global Registry\n";
    return;
  }
  if (!access_list_->checkAccess(reciever_name)) {
    std::cout << "[WARN] Process: " << reciever_name << " not in access list"
              << "\n";
    std::cout << "[ERROR]: Cannot send Message to: " << reciever_name
              << " as it is not in Access List\n";
    return;
  }
  if (!access_list_->check_connection(reciever_name)) {
    std::cout << "[WARN] Process: " << reciever_name
              << " not in connection list" << "\n";
    std::cout << "[ERROR]: Cannot send Message to: " << reciever_name
              << " as it is not in Connection List\n";
    Message con_req_message = MessageUtils::construct_CON_REQ(msg);
    con_req_message.header.seq_num =
        seq_number_.load(std::memory_order_relaxed);
    con_req_message.header.sender = self_proc_name_;
    con_req_message.header.reciever = reciever_name;
    std::vector<uint64_t> k(key_.begin(), key_.end());
    if (!MessageUtils::validate_message_sent(con_req_message, self_proc_name_,
                                             con_req_message.header.sender)) {
      std::cout << "[ERROR]: Messsage validation! Before. \n";
      MessageUtils::print_message(con_req_message);
      return;
    }
    EncryptionManager::Encrypt(con_req_message, k);
    BinaryManager::writeBinary(con_req_message, speed_dir_, seq_number_,
                               con_req_message.header.sender);
    seq_number_.fetch_add(1, std::memory_order_relaxed);
    std::cout << "[INFO]: Connection Request sent to: " << reciever_name
              << "\n";
  }
  Message message = MessageUtils::construct_MSG(msg);

  // load numeric seq value from atomic
  message.header.seq_num = seq_number_.load(std::memory_order_relaxed);

  message.header.sender = self_proc_name_;
  message.header.reciever = reciever_name;
  std::vector<uint64_t> k(key_.begin(), key_.end());
  if (!MessageUtils::validate_message_sent(message, self_proc_name_,
                                           reciever_name)) {
    std::cout << "[ERROR] Message validation failed! Before." << "\n";
    MessageUtils::print_message(message);
    return;
  }
  EncryptionManager::Encrypt(message, k);
  BinaryManager::writeBinary(message, speed_dir_, seq_number_, reciever_name);

  // advance the sender's local sequence number
  seq_number_.fetch_add(1, std::memory_order_relaxed);
}
void SPEED::runWatcherLoop_() {
  while (!watcher_should_exit_.load()) {
    // Scan all new files in our inbox directory
    for (auto &entry : std::filesystem::directory_iterator(self_speed_dir_)) {
      if (!entry.is_regular_file())
        continue;

      auto info = extractFileInfoFromFilename_(entry.path());
      if (!info.has_value())
        continue;

      const std::string fname = entry.path().filename().string();

      // Deduplicate so we don't enqueue the same file twice
      {
        std::lock_guard<std::mutex> seen_lock(seen_mutex_);
        if (seen_.count(fname))
          continue;
        seen_.insert(fname);
      }

      // Build a task and immediately hand it off to the per-receiver executor.
      // This implements "per-thread-per-receiver": each remote (proc_name)
      // gets its own worker that processes that remote's files sequentially.
      Task t;
      t.path = entry.path();
      t.seq = info->seq; // optional metadata

      enqueueTaskForSender(info->proc_name, std::move(t));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void SPEED::processFile_(const std::filesystem::path &file_path) {
  Message msg;
  try {
    msg = BinaryManager::readBinary(file_path);
  } catch (const std::exception &e) {
    std::cerr << "[SPEED] Failed to read file " << file_path << ": " << e.what()
              << "\n";
    std::lock_guard<std::mutex> lk(seen_mutex_);
    seen_.erase(file_path.filename().string());
    return;
  }

  std::vector<uint64_t> k(key_.begin(), key_.end());
  try {
    EncryptionManager::Decrypt(msg, k);
  } catch (const std::exception &e) {
    std::cerr << "[SPEED] Decrypt failed for " << file_path << ": " << e.what()
              << "\n";
    std::lock_guard<std::mutex> lk(seen_mutex_);
    seen_.erase(file_path.filename().string());
    return;
  }

  if (!MessageUtils::validate_message_recieved(msg, self_proc_name_)) {
    std::cerr << "[ERROR]: Invalid Message received! Not Processing.\n";
    MessageUtils::print_message(msg);
    std::lock_guard<std::mutex> lk(seen_mutex_);
    seen_.erase(file_path.filename().string());
    return;
  }

  // Handle message types
  switch (msg.header.type) {
  case MessageType::MSG: {
    PMessage mm = MessageUtils::destruct_message(msg);
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_(mm);
    break;
  }
  case MessageType::EXIT_NOTIF: {
    std::lock_guard<std::mutex> lk(access_list_mutex_);
    access_list_->removeProcessFromGlobalRegistry(msg.header.sender);
    access_list_->removeProcessFromAccessList(msg.header.sender);
    access_list_->removeProcessFromConnectedList(msg.header.sender);
    std::cout << "[DEBUG]: Received EXIT_NOTIF for: " << msg.header.sender
              << "\n";
    break;
  }
  case MessageType::CON_REQ: {
    std::cout << "\n[INFO]: Recieved Connection Request from: "
              << msg.header.sender << "\n";
    auto accessList = access_list_->getAccessList();
    if (accessList.find(msg.header.sender) != accessList.end()) {
      std::cout << "\n[INFO]: Found sender: " << msg.header.sender
                << " in access list\n";
      Message message = MessageUtils::construct_CON_RES(msg.header.sender);
      message.header.seq_num = seq_number_.load(std::memory_order_relaxed);
      message.header.sender = self_proc_name_;
      message.header.reciever = msg.header.sender;
      std::vector<uint64_t> k(key_.begin(), key_.end());
      if (!MessageUtils::validate_message_sent(message, self_proc_name_,
                                               msg.header.sender)) {
        std::cout << "[ERROR]: Messsage validation! Before. \n";
        MessageUtils::print_message(message);
        break;
      }
      EncryptionManager::Encrypt(message, k);
      BinaryManager::writeBinary(message, speed_dir_, seq_number_,
                                 msg.header.sender);
      seq_number_.fetch_add(1, std::memory_order_relaxed);
    } else {
      std::cout << "[ERROR]: Did not find sender: " << msg.header.sender
                << " in access list\n";
    }
    break;
  }
  case MessageType::CON_RES: {
    std::lock_guard<std::mutex> lk(con_res_buffer_mtx_);
    std::cout << "\n[INFO]: Recieved Connection Response from: "
              << msg.header.sender << "\n";
    const std::string con_response_from = msg.header.sender;
    if (con_res_buffer.find(con_response_from) != con_res_buffer.end()) {
      std::cout << "\n[INFO]: Found Connection Response In Buffer from: "
                << msg.header.sender << "\n";
      con_res_buffer.erase(con_response_from);
      access_list_->connect_to(con_response_from);
    }
    break;
  }
  case MessageType::PING:
    pong(msg.header.sender);
    break;
  case MessageType::PONG: {
    PMessage mm = MessageUtils::destruct_message(msg);
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_(mm);
    break;
  }
  default:
    break;
  }

  // Always remove file safely
  std::error_code ec;
  std::filesystem::remove(file_path, ec);
  if (ec)
    std::cerr << "[WARN]: Failed to remove file: " << file_path << " -> "
              << ec.message() << "\n";

  std::lock_guard<std::mutex> lock(seen_mutex_);
  seen_.erase(file_path.filename().string());
}

void SPEED::enqueueTaskForSender(const std::string &sender, Task task) {
  std::unique_lock<std::mutex> maplock(executors_mtx_);
  auto &ptr = executors_[sender];
  if (!ptr)
    ptr = std::make_unique<PerSenderExecutor>();
  PerSenderExecutor *pe = ptr.get();
  maplock.unlock();

  {
    std::unique_lock<std::mutex> lk(pe->m);
    pe->q.push_back(std::move(task));
    pe->cv.notify_one();
  }

  bool need_start = false;
  if (!pe->running.load(std::memory_order_acquire)) {
    bool expected = false;
    if (pe->running.compare_exchange_strong(expected, true))
      need_start = true;
  }

  if (need_start) {
    pe->stop_flag.store(false);
    pe->worker = std::make_unique<std::thread>([this, pe, sender]() {
      while (!pe->stop_flag.load()) {
        Task t;
        {
          std::unique_lock<std::mutex> lk(pe->m);
          pe->cv.wait_for(lk, pe->idle_timeout, [&] {
            return !pe->q.empty() || pe->stop_flag.load();
          });

          if (pe->stop_flag.load())
            break;

          if (pe->q.empty())
            continue; // nothing to process, go back to wait
          t = std::move(pe->q.front());
          pe->q.pop_front();
        }

        try {
          this->processFile_(t.path);
        } catch (const std::exception &e) {
          std::cerr << "[SPEED] Worker error for sender " << sender << ": "
                    << e.what() << "\n";
        }
      }
      pe->running.store(false);
    });

    pe->worker->detach();
  }
}

void SPEED::watcherSingleThread_() {
  runWatcherLoop_(); // Blocking call
}

void SPEED::watcherMultiThread_() {
  runWatcherLoop_(); // Non-blocking because start() already spawns thread
}

std::optional<SPEED::ParsedFileInfo>
SPEED::extractFileInfoFromFilename_(const std::filesystem::path &path) const {
  static const std::regex re(
      R"((\d+)_([A-Za-z0-9_]+)_(\d+)_([A-Za-z0-9-]+)\.ospeed)");
  std::smatch m;
  std::string filename = path.filename().string();
  if (std::regex_match(filename, m, re)) {
    try {
      return ParsedFileInfo{m[2].str(),             // proc_name
                            std::stoll(m[3].str()), // seq
                            path};
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

void SPEED::ping(const std::string &reciever_name) { ping_(reciever_name); }
void SPEED::pong(const std::string &reciever_name) { pong_(reciever_name); }
void SPEED::ping_(const std::string &reciever_name) {
  Message ping_message = MessageUtils::construct_PING(reciever_name);
  ping_message.header.seq_num = seq_number_;
  ping_message.header.sender = self_proc_name_;
  std::vector<uint64_t> k(key_.begin(), key_.end());
  EncryptionManager::Encrypt(ping_message, k);
  BinaryManager::writeBinary(ping_message, speed_dir_, seq_number_,
                             reciever_name);
  seq_number_.fetch_add(1, std::memory_order_relaxed);
}
void SPEED::pong_(const std::string &reciever_name) {
  Message pong_message = MessageUtils::construct_PONG(reciever_name);
  pong_message.header.seq_num = seq_number_;
  pong_message.header.sender = self_proc_name_;
  std::cout << "\n[INFO]: Sending a PONG to: " << reciever_name << "\n";
  std::vector<uint64_t> k(key_.begin(), key_.end());
  EncryptionManager::Encrypt(pong_message, k);
  BinaryManager::writeBinary(pong_message, speed_dir_, seq_number_,
                             reciever_name);
  seq_number_.fetch_add(1, std::memory_order_relaxed);
}
void SPEED::registerMethod(const std::string &name, RemoteFunction func) {
  function_registry_[name] = std::move(func);
}
void SPEED::invokeMethod(const std::string &name,
                         const std::vector<std::string> &args) {
  if (function_registry_.count(name)) {
    function_registry_[name](args);
  } else {
    std::cerr << "[ERROR] Method '" << name << "' not found.\n";
  }
}

void SPEED::stopAllExecutors() {
  std::lock_guard<std::mutex> maplock(executors_mtx_);
  for (auto &[sender, exec] : executors_) {
    exec->stop_flag.store(true);
    exec->cv.notify_all();
  }
  executors_.clear();
}

} // namespace SPEED
