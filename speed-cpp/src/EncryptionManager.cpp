#include "../include/EncryptionManager.hpp"
#include <iostream>
#include <sodium.h>

namespace SPEED {
// replace your Encrypt and Decrypt implementations with the following:

void EncryptionManager::Encrypt(Message &msg,
                                const std::vector<uint64_t> &key) {
  if (sodium_init() < 0) {
    throw std::runtime_error("libsodium init failed");
  }

  constexpr size_t KEY_BYTES = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
  constexpr size_t NONCE_BYTES = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
  constexpr size_t TAG_BYTES = crypto_aead_xchacha20poly1305_ietf_ABYTES;

  // Validate nonce container capacity
  if (msg.header.nonce.size() != NONCE_BYTES) {
    std::cerr << "[ERROR] EncryptionManager::Encrypt: Message header nonce "
                 "array has unexpected size.\n";
    throw std::runtime_error("Invalid nonce buffer size");
  }

  // Convert vector<uint64_t> -> deterministic byte sequence (little-endian)
  std::vector<unsigned char> key_bytes;
  key_bytes.reserve(key.size() * sizeof(uint64_t));
  for (uint64_t v : key) {
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
      key_bytes.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
    }
  }

  // Derive a fixed-length real_key from whatever key material user provided.
  unsigned char real_key[KEY_BYTES];
  if (crypto_generichash(real_key, KEY_BYTES, key_bytes.data(),
                         key_bytes.size(), nullptr, 0) != 0) {
    sodium_memzero(real_key, sizeof(real_key));
    throw std::runtime_error("Key derivation failed");
  }
  // wipe temporary key material
  sodium_memzero(key_bytes.data(), key_bytes.size());

  // Generate base per-message nonce (stored in header) — random per message.
  randombytes_buf(msg.header.nonce.data(), NONCE_BYTES);

  // helper that produces a per-field nonce by writing a 64-bit counter into
  // the last 8 bytes of the base nonce (little-endian). This avoids nonce
  // reuse while keeping the base nonce in the header so receiver can reproduce.
  auto make_field_nonce = [&](uint64_t counter,
                              std::array<uint8_t, NONCE_BYTES> &out) {
    static_assert(NONCE_BYTES >= 8, "nonce too small");
    std::copy(msg.header.nonce.begin(), msg.header.nonce.end(), out.begin());
    // write counter into last 8 bytes little-endian
    for (size_t i = 0; i < 8; ++i) {
      out[NONCE_BYTES - 8 + i] =
          static_cast<uint8_t>((counter >> (8 * i)) & 0xFF);
    }
  };

  uint64_t field_counter =
      1; // start counters at 1, increment only when encrypting a field

  auto encrypt_string = [&](const std::string &input,
                            const char *field_name) -> std::string {
    if (input.empty())
      return std::string();

    std::vector<unsigned char> ciphertext(input.size() + TAG_BYTES);
    unsigned long long clen = 0;

    std::array<uint8_t, NONCE_BYTES> fnonce;
    make_field_nonce(field_counter, fnonce);

    int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
        ciphertext.data(), &clen,
        reinterpret_cast<const unsigned char *>(input.data()), input.size(),
        nullptr, 0, // no additional data
        nullptr, fnonce.data(), real_key);

    if (rc != 0) {
      std::cerr
          << "[ERROR] EncryptionManager::Encrypt: encryption failed for field '"
          << field_name << "'. libsodium returned " << rc << "\n";
      sodium_memzero(real_key, sizeof(real_key));
      sodium_memzero(ciphertext.data(), ciphertext.size());
      throw std::runtime_error("Encryption failed");
    }

    // success => increment the per-field counter for next field
    ++field_counter;

    std::string out(reinterpret_cast<char *>(ciphertext.data()),
                    static_cast<size_t>(clen));
    sodium_memzero(ciphertext.data(), ciphertext.size());
    return out;
  };

  try {
    if (!msg.header.sender.empty()) {
      msg.header.sender = encrypt_string(msg.header.sender, "sender");
    }
    if (!msg.header.reciever.empty()) {
      msg.header.reciever = encrypt_string(msg.header.reciever, "reciever");
    }

    if (!msg.payload.empty()) {
      std::vector<unsigned char> ciphertext(msg.payload.size() + TAG_BYTES);
      unsigned long long clen = 0;

      std::array<uint8_t, NONCE_BYTES> fnonce;
      make_field_nonce(field_counter, fnonce);

      int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
          ciphertext.data(), &clen, msg.payload.data(), msg.payload.size(),
          nullptr, 0, nullptr, fnonce.data(), real_key);

      if (rc != 0) {
        std::cerr << "[ERROR] EncryptionManager::Encrypt: payload encryption "
                     "failed. libsodium returned "
                  << rc << "\n";
        sodium_memzero(real_key, sizeof(real_key));
        sodium_memzero(ciphertext.data(), ciphertext.size());
        throw std::runtime_error("Payload encryption failed");
      }

      ++field_counter; // consumed

      msg.payload.assign(ciphertext.begin(),
                         ciphertext.begin() + static_cast<size_t>(clen));
      sodium_memzero(ciphertext.data(), ciphertext.size());
    }
  } catch (...) {
    sodium_memzero(real_key, sizeof(real_key));
    throw;
  }

  // Zero key material on stack
  sodium_memzero(real_key, sizeof(real_key));
}

void EncryptionManager::Decrypt(Message &msg,
                                const std::vector<uint64_t> &key) {
  if (sodium_init() < 0) {
    throw std::runtime_error("libsodium init failed");
  }

  constexpr size_t KEY_BYTES = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
  constexpr size_t NONCE_BYTES = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
  constexpr size_t TAG_BYTES = crypto_aead_xchacha20poly1305_ietf_ABYTES;

  if (msg.header.nonce.size() != NONCE_BYTES) {
    std::cerr << "[ERROR] EncryptionManager::Decrypt: Message header nonce has "
                 "unexpected size.\n";
    throw std::runtime_error("Invalid nonce buffer size");
  }

  // Convert vector<uint64_t> -> deterministic byte sequence (little-endian)
  std::vector<unsigned char> key_bytes;
  key_bytes.reserve(key.size() * sizeof(uint64_t));
  for (uint64_t v : key) {
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
      key_bytes.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
    }
  }

  unsigned char real_key[KEY_BYTES];
  if (crypto_generichash(real_key, KEY_BYTES, key_bytes.data(),
                         key_bytes.size(), nullptr, 0) != 0) {
    sodium_memzero(real_key, sizeof(real_key));
    throw std::runtime_error("Key derivation failed");
  }
  sodium_memzero(key_bytes.data(), key_bytes.size());

  // helper as in Encrypt: reconstruct the same per-field nonces
  auto make_field_nonce = [&](uint64_t counter,
                              std::array<uint8_t, NONCE_BYTES> &out) {
    std::copy(msg.header.nonce.begin(), msg.header.nonce.end(), out.begin());
    for (size_t i = 0; i < 8; ++i) {
      out[NONCE_BYTES - 8 + i] =
          static_cast<uint8_t>((counter >> (8 * i)) & 0xFF);
    }
  };

  uint64_t field_counter = 1;

  auto decrypt_string = [&](const std::string &ciphertext,
                            const char *field_name) -> std::string {
    if (ciphertext.empty())
      return std::string();

    std::vector<unsigned char> plaintext(ciphertext.size()); // worst-case
    unsigned long long plen = 0;

    std::array<uint8_t, NONCE_BYTES> fnonce;
    make_field_nonce(field_counter, fnonce);

    int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
        plaintext.data(), &plen, nullptr,
        reinterpret_cast<const unsigned char *>(ciphertext.data()),
        ciphertext.size(), nullptr, 0, fnonce.data(), real_key);

    if (rc != 0) {
      sodium_memzero(plaintext.data(), plaintext.size());
      std::cerr << "[ERROR] EncryptionManager::Decrypt: decryption/auth failed "
                   "for field '"
                << field_name << "'. libsodium rc=" << rc << "\n";
      throw std::runtime_error("String decryption failed (auth error)");
    }

    ++field_counter;

    std::string out(reinterpret_cast<char *>(plaintext.data()),
                    static_cast<size_t>(plen));
    sodium_memzero(plaintext.data(), plaintext.size());
    return out;
  };

  try {
    if (!msg.header.sender.empty()) {
      msg.header.sender = decrypt_string(msg.header.sender, "sender");
    }
    if (!msg.header.reciever.empty()) {
      msg.header.reciever = decrypt_string(msg.header.reciever, "reciever");
    }

    if (!msg.payload.empty()) {
      std::vector<unsigned char> plaintext(msg.payload.size());
      unsigned long long plen = 0;

      std::array<uint8_t, NONCE_BYTES> fnonce;
      make_field_nonce(field_counter, fnonce);

      int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
          plaintext.data(), &plen, nullptr, msg.payload.data(),
          msg.payload.size(), nullptr, 0, fnonce.data(), real_key);

      if (rc != 0) {
        sodium_memzero(plaintext.data(), plaintext.size());
        std::cerr << "[ERROR] EncryptionManager::Decrypt: payload "
                     "decryption/auth failed. libsodium rc="
                  << rc << "\n";
        throw std::runtime_error("Payload decryption failed (auth error)");
      }

      ++field_counter;

      msg.payload.assign(plaintext.begin(),
                         plaintext.begin() + static_cast<size_t>(plen));
      sodium_memzero(plaintext.data(), plaintext.size());
    }
  } catch (...) {
    sodium_memzero(real_key, sizeof(real_key));
    throw;
  }

  sodium_memzero(real_key, sizeof(real_key));
}

} // namespace SPEED
