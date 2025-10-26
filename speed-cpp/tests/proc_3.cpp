#include "../include/SPEED.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  SPEED::SPEED spd("Proc_3", SPEED::ThreadMode::Multi);
  spd.addProcess("Proc_1");
  spd.setKeyFile("/home/jasper/Development/SPEED/speed-cpp/config.key");
  spd.start();

  std::cout << "[Proc_3] Sending messages to Proc_1...\n";
  for (int i = 1; i <= 50; ++i) {
    std::string msg = "PROC 2 Message_ i = " + std::to_string(i);
    spd.sendMessage(msg, "Proc_1");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "[Proc_3] Done sending.\n";
  std::this_thread::sleep_for(std::chrono::seconds(3));
  // spd.kill();
}
