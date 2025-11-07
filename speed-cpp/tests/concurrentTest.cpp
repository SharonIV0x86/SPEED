#include "../include/SPEED.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace SPEED;
namespace fs = std::filesystem;

class Receiver {
public:
  void onMessage(const std::vector<std::string> &args) {
    if (args.empty())
      return;
    auto sender = args[0];
    auto msg = args.size() > 1 ? args[1] : "";
    auto delay = args.size() > 2 ? std::stoi(args[2]) : 0;

    std::cout << "[" << sender << "] Received: " << msg << " (sleep " << delay
              << "s)" << std::endl;

    if (delay > 0)
      std::this_thread::sleep_for(std::chrono::seconds(delay));

    std::cout << "[" << sender << "] Done: " << msg << std::endl;
  }
};

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: ./proc_<id> <proc_name>\n";
    return 1;
  }

  std::string proc_name = argv[1];
  ThreadMode mode = ThreadMode::Multi;

  // Base test directory
  fs::path base_dir = fs::current_path() / "speed_test_dir";
  fs::create_directories(base_dir);

  SPEED::SPEED speed(proc_name, mode, base_dir);
  Receiver receiver;
  speed.registerMethod("onMessage", &Receiver::onMessage, &receiver);
  speed.start();

  std::this_thread::sleep_for(std::chrono::seconds(1));

  // === SENDER PROCESSES ===
  // Each process can send to others concurrently.
  if (proc_name == "proc_1") {
    speed.addProcess("proc_2");
    speed.addProcess("proc_3");

    // sender 1: normal messages
    for (int i = 0; i < 5; ++i) {
      speed.sendMessage("proc_2", "proc_1|msg_" + std::to_string(i) + "|0");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // slow sender to proc_3
    for (int i = 0; i < 3; ++i) {
      speed.sendMessage("proc_3", "proc_1|slow_" + std::to_string(i) + "|2");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  } else if (proc_name == "proc_2") {
    speed.addProcess("proc_1");
    speed.addProcess("proc_3");

    for (int i = 0; i < 5; ++i) {
      speed.sendMessage("proc_1", "proc_2|msg_" + std::to_string(i) + "|0");
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  } else if (proc_name == "proc_3") {
    speed.addProcess("proc_1");
    speed.addProcess("proc_2");

    for (int i = 0; i < 5; ++i) {
      speed.sendMessage("proc_1", "proc_3|msg_" + std::to_string(i) + "|0");
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }

  // Let all tasks drain
  std::cout << "[" << proc_name << "] Waiting for messages..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(10));

  speed.kill();
  std::cout << "[" << proc_name << "] Exiting cleanly.\n";
  return 0;
}
