#pragma once
#include "Constants.hpp"
#include "Utils.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
namespace SPEED {
enum class MessageType {
  MSG,
  CON_REQ,
  CON_RES,
  INVOKE_METHOD,
  EXIT_NOTIF,
  PING,
  PONG
};
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
struct PMessage {
public:
  std::string sender_name;
  std::string message;
  uint64_t timestamp;
  uint64_t sequence_num;
  PMessage(const std::string &sender_name, const std::string &message,
           const uint64_t &timestamp, const uint64_t &sequence_num) {
    this->sender_name = sender_name;
    this->message = message;
    this->timestamp = timestamp;
    this->sequence_num = sequence_num;
  }
  PMessage() {};
};

struct Message {
  MessageHeader header;
  std::vector<uint8_t> payload;
};
} // namespace SPEED
