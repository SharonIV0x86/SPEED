#include "../include/EncryptionManager.hpp"

namespace SPEED {
void EncryptionManager::Encrypt(Message &msg,
                                const std::vector<uint64_t> &key) {
  if (sodium_init() < 0) {
    throw std::runtime_error("libsodium init failed");
  }

  unsigned char real_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES] = {0};
  size_t copy_len = std::min(sizeof(real_key), key.size() * sizeof(uint64_t));
  memcpy(real_key, key.data(), copy_len);

  randombytes_buf(msg.header.nonce.data(), msg.header.nonce.size());

  auto encrypt_string = [&](const std::string &input) -> std::string {
    std::vector<unsigned char> ciphertext(
        input.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);

    unsigned long long clen;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext.data(), &clen,
            reinterpret_cast<const unsigned char *>(input.data()), input.size(),
            nullptr, 0, nullptr, msg.header.nonce.data(), real_key) != 0) {
      throw std::runtime_error("Encryption failed");
    }
    return std::string(reinterpret_cast<char *>(ciphertext.data()), clen);
  };

  msg.header.sender = encrypt_string(msg.header.sender);
  msg.header.reciever = encrypt_string(msg.header.reciever);

  if (!msg.payload.empty()) {
    std::vector<unsigned char> ciphertext(
        msg.payload.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);

    unsigned long long clen;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext.data(), &clen, msg.payload.data(), msg.payload.size(),
            nullptr, 0, nullptr, msg.header.nonce.data(), real_key) != 0) {
      throw std::runtime_error("Payload encryption failed");
    }

    msg.payload.assign(ciphertext.begin(), ciphertext.begin() + clen);
  }
}
void EncryptionManager::Decrypt(Message &msg,
                                const std::vector<uint64_t> &key) {
  if (sodium_init() < 0) {
    throw std::runtime_error("libsodium init failed");
  }

  unsigned char real_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES] = {0};
  size_t copy_len = std::min(sizeof(real_key), key.size() * sizeof(uint64_t));
  memcpy(real_key, key.data(), copy_len);

  auto decrypt_string = [&](const std::string &ciphertext) -> std::string {
    std::vector<unsigned char> plaintext(ciphertext.size());
    unsigned long long plen;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext.data(), &plen, nullptr,
            reinterpret_cast<const unsigned char *>(ciphertext.data()),
            ciphertext.size(), nullptr, 0, msg.header.nonce.data(),
            real_key) != 0) {
      throw std::runtime_error("String decryption failed (auth error)");
    }

    return std::string(reinterpret_cast<char *>(plaintext.data()), plen);
  };

  if (!msg.header.sender.empty()) {
    msg.header.sender = decrypt_string(msg.header.sender);
  }
  if (!msg.header.reciever.empty()) {
    msg.header.reciever = decrypt_string(msg.header.reciever);
  }

  if (!msg.payload.empty()) {
    std::vector<unsigned char> plaintext(msg.payload.size());
    unsigned long long plen;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext.data(), &plen, nullptr, msg.payload.data(),
            msg.payload.size(), nullptr, 0, msg.header.nonce.data(),
            real_key) != 0) {
      throw std::runtime_error("Payload decryption failed (auth error)");
    }

    msg.payload.assign(plaintext.begin(), plaintext.begin() + plen);
  }
}

} // namespace SPEED