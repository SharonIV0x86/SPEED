#pragma once
#include "AccessRegistry.hpp"
#include "BinaryManager.hpp"
#include "BinaryMessage.hpp"
#include "Constants.hpp"
#include "EncryptionManager.hpp"
#include "KeyManager.hpp"
#include "MessageUtils.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>
namespace SPEED {

enum class ThreadMode { Single = 0, Multi = 1 };

class SPEED {
public:
  using RemoteFunction = std::function<void(const std::vector<std::string> &)>;

  void sendMessage(const std::string &, const std::string &);
  void kill();
  void stop();
  void resume();
  void start();
  void registerMethod(const std::string &, RemoteFunction);
  template <typename T>
  void registerMethod(const std::string &name,
                      void (T::*method)(const std::vector<std::string> &),
                      T *instance) {
    function_registry_[name] = [instance,
                                method](const std::vector<std::string> &args) {
      (instance->*method)(args);
    };
  }
  void invokeMethod(const std::string &, const std::vector<std::string> &);
  bool addProcess(const std::string &);
  void ping(const std::string &);
  void pong(const std::string &);
  SPEED(const std::string &, const ThreadMode &, const std::filesystem::path &);
  SPEED(const std::string &, const ThreadMode &);
  void printGlobalRegistry_() {
    std::cout << "----------Global Registry----------\n";
    auto gr = access_list_->getGlobalRegistry();
    for (const auto &entry : gr) {
      std::cout << entry << "\n";
    }
    std::cout << "----------Global Registry----------\n";
  }

  void printAccessList_() {
    std::cout << "----------Access List----------\n";
    auto gr = access_list_->getAccessList();
    for (const auto &entry : gr) {
      std::cout << entry << "\n";
    }
    std::cout << "----------Access List----------\n";
  }

  void printConnectedList_() {
    std::cout << "----------Connected List----------\n";
    auto gr = access_list_->getConnectedList();
    for (const auto &entry : gr) {
      std::cout << entry << "\n";
    }
    std::cout << "----------Connected List----------\n";
  }

  bool setKeyFile(const std::filesystem::path &);
  void setCallback(std::function<void(const PMessage &)> cb);
  ~SPEED();

private:
  struct ParsedFileInfo {
    std::string proc_name;
    long long seq;
    std::filesystem::path path;
  };
  struct Task {
    PMessage msg;
    std::filesystem::path path;
    long long seq;
    Task() {};
  };

  struct PerSenderExecutor {
    std::deque<Task> q;
    std::mutex m;
    std::condition_variable cv;
    std::unique_ptr<std::thread> worker;
    std::atomic<bool> running{false};
    std::atomic<bool> stop_flag{false};
    size_t max_q = 256;
    std::chrono::milliseconds idle_timeout{5000};
  };

  ThreadMode tmode_;
  std::filesystem::path speed_dir_;
  std::filesystem::path self_speed_dir_;

  std::string key_;
  std::string self_proc_name_;
  std::filesystem::path key_path_;
  std::atomic<long long> seq_number_{0};

  std::function<void(const PMessage &)> callback_;
  std::unique_ptr<AccessRegistry> access_list_;

  std::mutex callback_mutex_;
  std::mutex access_list_mutex_;
  std::mutex key_mutex_;
  std::mutex seen_mutex_;
  std::mutex heap_mutex_;
  std::mutex single_mtx_;
  std::mutex multi_mutex_;
  std::mutex write_mutex_;
  std::mutex rfi_mutex_;
  std::mutex watcher_mutex_;
  std::mutex fifo_mutex_;
  std::mutex executors_mtx_;
  std::mutex con_res_buffer_mtx_;
  std::thread watcher_thread_;
  std::atomic<bool> watcher_running_{false};
  std::atomic<bool> watcher_should_exit_{false};

  void watcherSingleThread_(); // blocking call for single-thread mode
  void watcherMultiThread_();  // non-blocking call for multi-thread mode
  void processFile_(const std::filesystem::path &file_path);
  void runWatcherLoop_(); // Core FIFO logic
  void enqueueTaskForSender(const std::string &, Task);
  void stopAllExecutors();
  void ping_(const std::string &);
  void pong_(const std::string &);

  std::optional<ParsedFileInfo>
  extractFileInfoFromFilename_(const std::filesystem::path &path) const;

  using FileCandidate = std::pair<long long, std::filesystem::path>;

  std::unordered_set<std::string> seen_;
  std::unordered_set<std::string> con_res_buffer;
  std::unordered_map<std::string, RemoteFunction> function_registry_;
  std::unordered_map<std::string, long long> next_expected_seq_;
  std::unordered_map<std::string, std::unique_ptr<PerSenderExecutor>>
      executors_;
  std::unordered_map<std::string, std::map<long long, std::filesystem::path>>
      sender_buffers_;
};

} // namespace SPEED
