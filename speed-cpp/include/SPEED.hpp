#pragma once
#include "AccessRegistry.hpp"
#include "BinaryMessage.hpp"
#include "KeyManager.hpp"
#include "Utils.hpp"
#include <atomic>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

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

private:
  std::string self_proc_name_;
  ThreadMode tmode_;
  std::filesystem::path speed_dir_;
  std::filesystem::path communication_dir_;

  std::string key_;
  std::filesystem::path key_path_;
  std::atomic<long long> seq_number_{0};

  std::function<void(const PMessage &)> callback_;
  std::unique_ptr<AccessRegistry> access_list_;

  mutable std::mutex callback_mutex_;
  mutable std::mutex access_list_mutex_;
  mutable std::mutex key_mutex_;

  void processIncomingMessage_(const Message &);
};
} // namespace SPEED
