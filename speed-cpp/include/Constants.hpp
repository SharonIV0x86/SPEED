#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace SOEED {

enum class MessageType : uint8_t {
  PING_IS_ALIVE = 0x01,
  INVOKE_METHOD = 0x02,
  ACTION = 0x03,
  ERROR = 0x05,
  CON_REQ = 0x06,
  CON_RES = 0x07
};

constexpr size_t MAX_ID_LENGTH = 255;
constexpr size_t SPEED_VERSION = 0x01;
constexpr size_t HEADER_FIXED_SIZE =
    24; // Version(1) + Type(1) + Flags(1) + Reserved(1) + PID(4) + Timestamp(8)
        // + SeqNum(8)

// Libsodium constants
constexpr size_t KEY_BYTES = 32; // crypto_aead_aes256gcm_KEYBYTES
} // namespace SOEED