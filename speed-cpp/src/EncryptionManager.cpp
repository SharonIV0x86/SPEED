#include "../include/EncryptionManager.hpp"
#include <iostream>
#include <sodium.h>

namespace SPEED {

void EncryptionManager::Encrypt(Message &msg,
                                const std::vector<uint64_t> &key) {
  if (sodium_init() < 0) {
    throw std::runtime_error("libsodium init failed");
  }

  // Sanity: required key length in bytes
  constexpr size_t KEY_BYTES = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
  constexpr size_t NONCE_BYTES = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
  constexpr size_t TAG_BYTES = crypto_aead_xchacha20poly1305_ietf_ABYTES;

  // Validate key length (fail if too short — avoids accidental zero-padded
  // keys)
  size_t key_bytes_available = key.size() * sizeof(uint64_t);
  if (key_bytes_available < KEY_BYTES) {
    std::cerr << "[ERROR] EncryptionManager::Encrypt: provided key length ("
              << key_bytes_available << " bytes) is less than required "
              << KEY_BYTES << " bytes. Aborting.\n";
    throw std::runtime_error("Insufficient key length for encryption");
  }

  // Fill real_key with first KEY_BYTES bytes of key vector
  unsigned char real_key[KEY_BYTES];
  std::memset(real_key, 0, sizeof(real_key));
  size_t copy_len = std::min(KEY_BYTES, key_bytes_available);
  std::memcpy(real_key, reinterpret_cast<const unsigned char *>(key.data()),
              copy_len);

  // Validate/ensure nonce buffer size
  if (msg.header.nonce.size() != NONCE_BYTES) {
    sodium_memzero(real_key, sizeof(real_key));
    std::cerr
        << "[ERROR] EncryptionManager::Encrypt: nonce size mismatch. Expected "
        << NONCE_BYTES << " bytes.\n";
    throw std::runtime_error("Invalid nonce size");
  }

  // Generate a fresh random nonce for this message
  randombytes_buf(msg.header.nonce.data(), msg.header.nonce.size());

  // Helper to encrypt std::string fields (sender/reciever)
  auto encrypt_string = [&](const std::string &input,
                            const char *field_name) -> std::string {
    std::vector<unsigned char> ciphertext(input.size() + TAG_BYTES);
    unsigned long long clen = 0;

    int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
        ciphertext.data(), &clen,
        reinterpret_cast<const unsigned char *>(input.data()), input.size(),
        nullptr, 0, // no additional data
        nullptr, msg.header.nonce.data(), real_key);

    if (rc != 0) {
      std::cerr
          << "[ERROR] EncryptionManager::Encrypt: encryption failed for field '"
          << field_name << "'. libsodium returned " << rc << "\n";
      // zero real key before throwing
      sodium_memzero(real_key, sizeof(real_key));
      throw std::runtime_error("Encryption failed");
    }

    // Construct std::string from ciphertext bytes (may contain nulls)
    std::string out(reinterpret_cast<char *>(ciphertext.data()),
                    static_cast<size_t>(clen));
    // zero ciphertext buffer for hygiene
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

      int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
          ciphertext.data(), &clen, msg.payload.data(), msg.payload.size(),
          nullptr, 0, nullptr, msg.header.nonce.data(), real_key);

      if (rc != 0) {
        std::cerr << "[ERROR] EncryptionManager::Encrypt: payload encryption "
                     "failed. libsodium returned "
                  << rc << "\n";
        sodium_memzero(real_key, sizeof(real_key));
        sodium_memzero(ciphertext.data(), ciphertext.size());
        throw std::runtime_error("Payload encryption failed");
      }

      // Assign ciphertext bytes into msg.payload
      msg.payload.assign(ciphertext.begin(),
                         ciphertext.begin() + static_cast<size_t>(clen));
      // wipe temporary ciphertext buffer
      sodium_memzero(ciphertext.data(), ciphertext.size());
    }
  } catch (...) {
    // Ensure key is zeroed before propagating
    sodium_memzero(real_key, sizeof(real_key));
    throw;
  }

  // Zero sensitive key material from stack
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

  size_t key_bytes_available = key.size() * sizeof(uint64_t);
  if (key_bytes_available < KEY_BYTES) {
    std::cerr << "[ERROR] EncryptionManager::Decrypt: provided key length ("
              << key_bytes_available << " bytes) is less than required "
              << KEY_BYTES << " bytes. Aborting.\n";
    throw std::runtime_error("Insufficient key length for decryption");
  }

  unsigned char real_key[KEY_BYTES];
  std::memset(real_key, 0, sizeof(real_key));
  size_t copy_len = std::min(KEY_BYTES, key_bytes_available);
  std::memcpy(real_key, reinterpret_cast<const unsigned char *>(key.data()),
              copy_len);

  if (msg.header.nonce.size() != NONCE_BYTES) {
    sodium_memzero(real_key, sizeof(real_key));
    std::cerr
        << "[ERROR] EncryptionManager::Decrypt: nonce size mismatch. Expected "
        << NONCE_BYTES << " bytes.\n";
    throw std::runtime_error("Invalid nonce size");
  }

  auto decrypt_string = [&](const std::string &ciphertext,
                            const char *field_name) -> std::string {
    if (ciphertext.empty())
      return {};

    std::vector<unsigned char> plaintext(ciphertext.size()); // worst-case
    unsigned long long plen = 0;

    int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
        plaintext.data(), &plen, nullptr,
        reinterpret_cast<const unsigned char *>(ciphertext.data()),
        ciphertext.size(), nullptr, 0, msg.header.nonce.data(), real_key);

    if (rc != 0) {
      // Auth failure (wrong key, corrupted data, wrong nonce)
      std::cerr << "[ERROR] EncryptionManager::Decrypt: decryption/auth failed "
                   "for field '"
                << field_name
                << "'. Likely wrong key or corrupted data (libsodium rc=" << rc
                << ").\n";
      // zero sensitive buffers
      sodium_memzero(plaintext.data(), plaintext.size());
      throw std::runtime_error("String decryption failed (auth error)");
    }

    std::string out(reinterpret_cast<char *>(plaintext.data()),
                    static_cast<size_t>(plen));
    // zero plaintext buffer
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
      std::vector<unsigned char> plaintext(msg.payload.size()); // worst-case
      unsigned long long plen = 0;

      int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
          plaintext.data(), &plen, nullptr, msg.payload.data(),
          msg.payload.size(), nullptr, 0, msg.header.nonce.data(), real_key);

      if (rc != 0) {
        std::cerr
            << "[ERROR] EncryptionManager::Decrypt: payload decryption/auth "
               "failed. Likely wrong key or corrupted data (libsodium rc="
            << rc << ").\n";
        sodium_memzero(plaintext.data(), plaintext.size());
        throw std::runtime_error("Payload decryption failed (auth error)");
      }

      msg.payload.assign(plaintext.begin(),
                         plaintext.begin() + static_cast<size_t>(plen));
      // zero plaintext buffer
      sodium_memzero(plaintext.data(), plaintext.size());
    }
  } catch (...) {
    // Zero key before rethrowing to avoid leaving sensitive data on the stack
    sodium_memzero(real_key, sizeof(real_key));
    throw;
  }

  // Zero key material on stack
  sodium_memzero(real_key, sizeof(real_key));
}

} // namespace SPEED
