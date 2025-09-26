#pragma once
#include "AccessRegistry.hpp"
#include "BinaryMessage.hpp"
#include "KeyManager.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

namespace SPEED {

enum class ThreadMode { Single = 0, Multi = 1 };

class SPEED {
public:
  void sendMessage();
  void kill();
  void stop();
  void resume();
  void start();

  SPEED(const std::string &, const ThreadMode &, const std::filesystem::path &);
  SPEED(const std::string &, const ThreadMode &);

  bool setKeyFile(const std::filesystem::path &);
  void setCallback(std::function<void(const PMessage &)> cb);
  void trigger();
  void addProcess(const std::string &);

  ~SPEED();

private:
  std::string self_proc_name_;
  ThreadMode tmode_;
  std::filesystem::path speed_dir_;
  std::filesystem::path self_speed_dir_;

  std::string key_;
  std::filesystem::path key_path_;
  std::atomic<long long> seq_number_{0};

  std::function<void(const PMessage &)> callback_;
  std::unique_ptr<AccessRegistry> access_list_;

  std::mutex callback_mutex_;
  std::mutex access_list_mutex_;
  std::mutex key_mutex_;

  std::thread watcher_thread_;
  std::atomic<bool> watcher_running_{false};
  std::atomic<bool> watcher_should_exit_{false};
  std::mutex watcher_mutex_;

  void processIncomingMessage_(const Message &);

  // internal watcher functions
  void watcherSingleThread_(); // blocking call for single-thread mode
  void watcherMultiThread_();  // non-blocking call for multi-thread mode

  void processFile_(const std::filesystem::path &file_path);
  std::optional<long long>
  extractSeqFromFilename_(const std::string &filename) const;
};

} // namespace SPEED
