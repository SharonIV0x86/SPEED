#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace SPEED {

constexpr size_t MAX_ID_LENGTH = 255;
constexpr size_t SPEED_VERSION = 0x01;
// Libsodium constants
constexpr size_t KEY_BYTES = 32; // crypto_aead_aes256gcm_KEYBYTES
} // namespace SPEED