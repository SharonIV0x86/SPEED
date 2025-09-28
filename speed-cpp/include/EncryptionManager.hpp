#pragma once
#include "BinaryMessage.hpp"
#include <cstring>
#include <iostream>
#include <sodium.h>
#include <vector>
namespace SPEED {
class EncryptionManager {
public:
  static void Encrypt(Message &, const std::vector<uint64_t> &);
  static void Decrypt(Message &, const std::vector<uint64_t> &);
};
} // namespace SPEED