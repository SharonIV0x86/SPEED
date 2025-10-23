#include "../include/BinaryManager.hpp"
#include "../include/Utils.hpp"
#include "../src/BinaryManager.cpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace SPEED;
namespace fs = std::filesystem;

class BinaryManagerTest : public ::testing::Test {
protected:
  fs::path tempDir;
  std::atomic<long long> seqNumber{0};
  std::string procName = "TestProc";

  void SetUp() override {
    tempDir = fs::temp_directory_path() / "binary_manager_test_dir";
    if (fs::exists(tempDir))
      fs::remove_all(tempDir);
    fs::create_directory(tempDir);
    fs::create_directory(tempDir / procName);
  }

  void TearDown() override {
    if (fs::exists(tempDir))
      fs::remove_all(tempDir);
  }

  Message makeSampleMessage() {
    Message msg;
    msg.header.version = 1;
    msg.header.type = MessageType::MSG;
    msg.header.sender_pid = 1234;
    msg.header.timestamp = 5678;
    msg.header.seq_num = seqNumber.load();
    msg.header.sender = "Alice";
    msg.header.reciever = "Bob";
    std::fill(msg.header.nonce.begin(), msg.header.nonce.end(), 0xAA);
    msg.payload = {'H', 'e', 'l', 'l', 'o'};
    return msg;
  }
};

// --- Tests ---

TEST_F(BinaryManagerTest, WriteAndReadBinaryBasic) {
  auto msg = makeSampleMessage();

  bool success = BinaryManager::writeBinary(msg, tempDir, seqNumber, procName);
  EXPECT_TRUE(success);

  // Find the written file (".ospeed")
  fs::path writtenFile;
  for (auto &entry : fs::directory_iterator(tempDir / procName)) {
    if (entry.path().extension() == ".ospeed") {
      writtenFile = entry.path();
      break;
    }
  }
  ASSERT_FALSE(writtenFile.empty());

  auto readMsg = BinaryManager::readBinary(writtenFile);

  EXPECT_EQ(readMsg.header.version, msg.header.version);
  EXPECT_EQ(readMsg.header.type, msg.header.type);
  EXPECT_EQ(readMsg.header.sender_pid, msg.header.sender_pid);
  EXPECT_EQ(readMsg.header.timestamp, msg.header.timestamp);
  EXPECT_EQ(readMsg.header.seq_num, msg.header.seq_num);
  EXPECT_EQ(readMsg.header.sender, msg.header.sender);
  EXPECT_EQ(readMsg.header.reciever, msg.header.reciever);
  EXPECT_EQ(readMsg.header.nonce, msg.header.nonce);
  EXPECT_EQ(readMsg.payload, msg.payload);
}

TEST_F(BinaryManagerTest, WriteBinaryEmptyPayload) {
  auto msg = makeSampleMessage();
  msg.payload.clear();

  bool success = BinaryManager::writeBinary(msg, tempDir, seqNumber, procName);
  EXPECT_TRUE(success);

  fs::path writtenFile;
  for (auto &entry : fs::directory_iterator(tempDir / procName))
    if (entry.path().extension() == ".ospeed")
      writtenFile = entry.path();

  ASSERT_FALSE(writtenFile.empty());

  auto readMsg = BinaryManager::readBinary(writtenFile);
  EXPECT_TRUE(readMsg.payload.empty());
}

TEST_F(BinaryManagerTest, ReadBinaryInvalidPathThrows) {
  EXPECT_THROW(BinaryManager::readBinary(tempDir / "nonexistent.ospeed"),
               std::runtime_error);
}

TEST_F(BinaryManagerTest, EndiannessConversion) {
  uint32_t val32 = 0x12345678;
  auto bytes32 = to_big_endian(val32);
  uint32_t val32_back = from_big_endian<uint32_t>(bytes32.data());
  EXPECT_EQ(val32, val32_back);

  uint64_t val64 = 0x123456789ABCDEF0;
  auto bytes64 = to_big_endian(val64);
  uint64_t val64_back = from_big_endian<uint64_t>(bytes64.data());
  EXPECT_EQ(val64, val64_back);
}

TEST_F(BinaryManagerTest, WriteAndReadStringFunctions) {
  std::string testStr = "Hello, World!";
  fs::path file = tempDir / "string_test.bin";

  // Write using the header utility functions
  std::ofstream out(file, std::ios::binary);
  SPEED::write_string(out, testStr);
  out.close();

  // Read using the header utility functions
  std::ifstream in(file, std::ios::binary);
  std::string readStr = SPEED::read_string(in);
  in.close();

  EXPECT_EQ(testStr, readStr);
}

TEST_F(BinaryManagerTest, SequenceNumberIncrements) {
  auto msg1 = makeSampleMessage();
  auto msg2 = makeSampleMessage();
  seqNumber = 100;

  bool s1 = BinaryManager::writeBinary(msg1, tempDir, seqNumber, procName);
  seqNumber++;
  bool s2 = BinaryManager::writeBinary(msg2, tempDir, seqNumber, procName);

  EXPECT_TRUE(s1);
  EXPECT_TRUE(s2);

  std::vector<std::string> files;
  for (auto &entry : fs::directory_iterator(tempDir / procName)) {
    if (entry.path().extension() == ".ospeed")
      files.push_back(entry.path().filename().string());
  }

  ASSERT_EQ(files.size(), 2);
  EXPECT_NE(files[0], files[1]); // filenames must be unique
}

TEST_F(BinaryManagerTest, WriteBinaryFileExists) {
  auto msg = makeSampleMessage();

  bool firstWrite =
      BinaryManager::writeBinary(msg, tempDir, seqNumber, procName);
  EXPECT_TRUE(firstWrite);

  bool secondWrite =
      BinaryManager::writeBinary(msg, tempDir, seqNumber, procName);
  EXPECT_TRUE(secondWrite); // should still succeed and generate unique file
}

TEST_F(BinaryManagerTest, NonexistentDirectoryFailsGracefully) {
  auto msg = makeSampleMessage();
  fs::path invalidDir = tempDir / "nonexistent_folder";

  bool success =
      BinaryManager::writeBinary(msg, invalidDir, seqNumber, procName);
  EXPECT_FALSE(success);
}
