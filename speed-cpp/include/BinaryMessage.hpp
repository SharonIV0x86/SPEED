#pragma once
#include <array>
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
  std::array<uint8_t, 24> nonce; // 24 bytes for XChaCha20-Poly1305
};

struct Message {
  MessageHeader header;
  std::vector<uint8_t> payload;
};
struct PMessage {
public:
  std::string sender_name;
  std::string message;
  std::string timestamp;
  PMessage(const std::string &sender_name, const std::string &message,
           const std::string &timestamp) {
    this->sender_name = sender_name;
    this->message = message;
    this->timestamp = timestamp;
  }
};

} // namespace SPEED
