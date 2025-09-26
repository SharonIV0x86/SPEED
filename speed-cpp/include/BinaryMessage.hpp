#pragma once
#include <cstdint>
#include <iostream>
#include <vector>
namespace SPEED {
enum class MessageType { MSG, CON_REQ, CON_RES, INVOKE_METHOD };
struct MessageHeader {
  uint8_t version;
  MessageType type;
  uint32_t sender_pid;
  uint64_t timestamp;
  uint64_t seq_num;
  std::string sender;
  std::string reciever;
};

struct Message {
  MessageHeader header;
  std::vector<uint8_t> payload;
};
struct PMessage {
public:
  std::string sender_name;
  std::string message;
  uint64_t timestamp;
  PMessage(const std::string &sender_name, const std::string &message,
           const uint64_t &timestamp) {
    this->sender_name = sender_name;
    this->message = message;
    this->timestamp = timestamp;
  }
};

} // namespace SPEED
