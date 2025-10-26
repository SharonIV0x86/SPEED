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
  static Message construct_CON_REQ(const std::string &reciever_name) {
    Message message;
    message.header.version = SPEED_VERSION;
    message.header.type = MessageType::CON_REQ;
    message.header.sender_pid = Utils::getProcessID();
    message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
    message.header.seq_num = -1;
    message.header.sender = "";
    message.header.reciever = reciever_name;

    const std::string m = "CON_REQ_";
    message.payload = std::vector<uint8_t>(m.begin(), m.end());
    return message;
  }
  static Message construct_CON_RES(const std::string &reciever_name) {
    Message message;
    message.header.version = SPEED_VERSION;
    message.header.type = MessageType::CON_RES;
    message.header.sender_pid = Utils::getProcessID();
    message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
    message.header.seq_num = -1;
    message.header.sender = "";
    message.header.reciever = reciever_name;

    const std::string m = "CON_RES_";
    message.payload = std::vector<uint8_t>(m.begin(), m.end());
    return message;
  }
  static Message construct_INVOKE_METHOD(const std::string &method_name,
                                         const std::string &reciever_name) {
    Message message;
    message.header.version = SPEED_VERSION;
    message.header.type = MessageType::INVOKE_METHOD;
    message.header.sender_pid = Utils::getProcessID();
    message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
    message.header.seq_num = -1;
    message.header.sender = "";
    message.header.reciever = reciever_name;

    message.payload =
        std::vector<uint8_t>(method_name.begin(), method_name.end());
    return message;
  }
  static Message construct_EXIT_NOTIF(const std::string &reciever_name) {
    Message message;
    message.header.version = SPEED_VERSION;
    message.header.type = MessageType::EXIT_NOTIF;
    message.header.sender_pid = Utils::getProcessID();
    message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
    message.header.seq_num = -1;
    message.header.sender = "";
    message.header.reciever = reciever_name;

    const std::string m = "EXIT_NOTIF_";
    message.payload = std::vector<uint8_t>(m.begin(), m.end());
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

    const std::string m = "PING";
    message.payload = std::vector<uint8_t>(m.begin(), m.end());

    return message;
  }
  static Message construct_PONG(const std::string &reciever_name) {
    Message message;
    message.header.version = SPEED_VERSION;
    message.header.type = MessageType::PONG;
    message.header.sender_pid = Utils::getProcessID();
    message.header.timestamp = std::stoull(Utils::getCurrentTimestamp());
    message.header.seq_num = -1;
    message.header.sender = "";
    message.header.reciever = reciever_name;

    const std::string m = "PONG";
    message.payload = std::vector<uint8_t>(m.begin(), m.end());
    return message;
  }
  static PMessage destruct_message(const Message &message) {
    const std::string m =
        std::string(message.payload.begin(), message.payload.end());
    PMessage message_(message.header.sender, m, message.header.timestamp,
                      message.header.seq_num);
    return message_;
  }
  static bool validate_message_sent(const Message &message,
                                    const std::string &self_proc_name,
                                    const std::string &reciever_name) {
    if (message.header.version != SPEED_VERSION) {
      std::cout << "[ERROR]: Mismatch version\n";
      return false;
    }
    if (message.header.sender != self_proc_name) {
      std::cout << "[ERROR]: Mismatch Sender\n";
      return false;
    }
    if (message.header.reciever != reciever_name) {
      std::cout << "[ERROR]: Mismatch Reciever\n";
      std::cout << "[RECIEVER MISMATCH]: EXPECTED: " << message.header.reciever
                << " GOT: " << reciever_name << "\n";
      return false;
    }
    return true;
  }
  static bool validate_message_recieved(const Message &message,
                                        const std::string &self_proc_name) {
    if (message.header.version != SPEED_VERSION) {
      std::cout << "[ERROR]: Mismatch version\n";
      return false;
    }
    // if (message.header.reciever != self_proc_name) {
    //   std::cout << "[ERROR]: Mismatch reciever\n";
    //   return false;
    // }
    return true;
  }
  static void print_message(const Message &message) {
    std::cout << "[PRINT] SPEED Version: " << message.header.version << "\n";
    std::cout << "[PRINT]: " << (int)message.header.type << "\n";
    std::cout << "[PRINT] Sender PID: " << message.header.sender_pid << "\n";
    std::cout << "[PRINT] Seq Number: " << message.header.seq_num << "\n";
    std::cout << "[PRINT] Sender: " << message.header.sender << "\n";
    std::cout << "[PRINT] Receiver: " << message.header.reciever << "\n";
    std::cout << "[PRINT] Payload: "
              << std::string(message.payload.begin(), message.payload.end())
              << "\n";
  }
};
} // namespace SPEED
