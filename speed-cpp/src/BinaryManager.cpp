#include "../include/BinaryManager.hpp"
#include <fstream>

namespace SPEED {

bool BinaryManager::writeBinary(const Message &msg,
                                const std::filesystem::path &path,
                                std::atomic<long long> &seq_number) {
  const long long seq = seq_number.load();
  std::filesystem::path before_path = path / (std::to_string(seq) + ".ispeed");
  std::filesystem::path after_path = path / (std::to_string(seq) + ".ospeed");
  std::ofstream out(before_path, std::ios::binary);
  if (!out)
    return false;

  write_uint(out, msg.header.version);
  write_uint(out, static_cast<uint8_t>(msg.header.type));
  write_uint(out, msg.header.sender_pid);
  write_uint(out, msg.header.timestamp);
  write_uint(out, msg.header.seq_num);
  write_string(out, msg.header.sender);
  write_string(out, msg.header.reciever);

  write_uint(out, static_cast<uint32_t>(msg.payload.size()));
  out.write(reinterpret_cast<const char *>(msg.payload.data()),
            msg.payload.size());
  std::rename(before_path.c_str(), after_path.c_str());
  return true;
}

Message BinaryManager::readBinary(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("Failed to open file");

  Message msg;
  msg.header.version = read_uint<uint8_t>(in);
  msg.header.type = static_cast<MessageType>(read_uint<uint8_t>(in));
  msg.header.sender_pid = read_uint<uint32_t>(in);
  msg.header.timestamp = read_uint<uint64_t>(in);
  msg.header.seq_num = read_uint<uint64_t>(in);
  msg.header.sender = read_string(in);
  msg.header.reciever = read_string(in);

  uint32_t len = read_uint<uint32_t>(in);
  msg.payload.resize(len);
  in.read(reinterpret_cast<char *>(msg.payload.data()), len);

  return msg;
}

} // namespace SPEED
