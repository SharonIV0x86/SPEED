#include "../include/SPEED.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>

// Track sequence numbers from each sender
std::unordered_map<std::string, uint64_t> lastSeq;

void user_callback(const SPEED::PMessage &msg) {
  auto sender = msg.sender_name;
  uint64_t seq = msg.sequence_num; // assuming SPEED::PMessage has this

  uint64_t prev = lastSeq[sender];
  if (prev != 0 && seq != prev + 1) {
    std::cerr << "[ERROR] Out of order message from " << sender << " (expected "
              << prev + 1 << ", got " << seq << ")\n";
  } else {
    std::cout << "[OK] Message " << seq << " from " << sender << " -> \""
              << msg.message << "\"\n";
  }
  lastSeq[sender] = seq;

  // Simulate slow callback for one sender to test non-blocking FIFO
  //   if (sender == "Proc_2" && seq == 2) {
  //     std::cout << "[INFO] Simulating slow handler for Proc_2 seq=2...\n";
  //     std::this_thread::sleep_for(std::chrono::seconds(2));
  //   }
}

int main() {
  SPEED::SPEED spd("Proc_1", SPEED::ThreadMode::Multi);
  spd.setCallback(user_callback);
  spd.addProcess("Proc_2");
  spd.addProcess("Proc_3");
  spd.setKeyFile("/home/jasper/Development/SPEED/speed-cpp/config.key");
  spd.start();

  std::cout << "[Proc_1] Listening for messages...\n";
  std::this_thread::sleep_for(std::chrono::seconds(100)); // Run test for 20s

  std::cout << "[Proc_1] Test complete, shutting down.\n";
  spd.kill();
}
