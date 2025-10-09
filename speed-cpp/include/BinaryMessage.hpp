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

struct Message {
  MessageHeader header;
  std::vector<uint8_t> payload;
  static Message construct_MSG(const std::string &msg) {
    Message message;
    message.header.version = SPEED_VERSION;
    message.header.type = MessageType::MSG;
    message.header.sender_pid = Utils::getProcessID();
    message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
    message.header.seq_num = -1;
    message.header.sender = "";
    message.header.reciever = "";
    message.payload = std::vector<uint8_t>(msg.begin(), msg.end());
    return message;
  }
  static Message construct_PING(const std::string &rec_name) {
    Message message;
    message.header.version = SPEED_VERSION;
    message.header.type = MessageType::PING;
    message.header.sender = Utils::getProcessID();
    message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
    message.header.seq_num = -1;
    message.header.sender = "";
    message.header.reciever = rec_name;
    const std::string m = "PONG";
    message.payload = std::vector<uint8_t>(m.begin(), m.end());

    return message;
  }
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
