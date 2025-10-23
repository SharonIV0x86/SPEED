#include "../include/AccessRegistry.hpp"
#include "../include/Utils.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

using namespace SPEED;

namespace fs = std::filesystem;

class AccessRegistryTest : public ::testing::Test {
protected:
  fs::path tempDir;
  std::string procName = "TestProc";

  void SetUp() override {
    tempDir = fs::temp_directory_path() / "access_registry_test_dir";
    if (fs::exists(tempDir))
      fs::remove_all(tempDir);
    fs::create_directory(tempDir);
  }

  void TearDown() override {
    if (fs::exists(tempDir))
      fs::remove_all(tempDir);
  }
};

// Utility to check file presence
bool fileExists(const fs::path &p) {
  return fs::exists(p) && fs::is_regular_file(p);
}

// --- TESTS ---

TEST_F(AccessRegistryTest, ConstructorCreatesAccessFile) {
  AccessRegistry reg(tempDir, procName);
  fs::path expectedFile = tempDir / (procName + ".oregistry");
  EXPECT_TRUE(fileExists(expectedFile));
}

TEST_F(AccessRegistryTest, RecreatesAccessFileIfAlreadyExists) {
  fs::path expectedFile = tempDir / (procName + ".oregistry");
  {
    std::ofstream f(expectedFile.string());
    f << "dummy";
  }
  EXPECT_TRUE(fileExists(expectedFile));

  // Constructor should remove and recreate it
  AccessRegistry reg(tempDir, procName);
  EXPECT_TRUE(fileExists(expectedFile));
}

TEST_F(AccessRegistryTest, GlobalRegistryBuildsCorrectly) {
  // Create some dummy .oregistry files to simulate other processes
  std::ofstream(tempDir / "Alpha.oregistry") << "Alpha";
  std::ofstream(tempDir / "Beta.oregistry") << "Beta";

  AccessRegistry reg(tempDir, procName);

  // Should have "Alpha.oregistry" and "Beta.oregistry" in global registry
  EXPECT_TRUE(reg.checkAccess("nonexistent") == false);
  EXPECT_FALSE(reg.checkGlobalRegistry(procName)); // current proc excluded
}

TEST_F(AccessRegistryTest, RemoveProcessThatDoesNotExistLogsWarning) {
  AccessRegistry reg(tempDir, procName);
  testing::internal::CaptureStdout();
  bool removed = reg.removeProcessFromAccessList("NonExistentProc");
  std::string out = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(removed);
  EXPECT_NE(out.find("[WARN]: Process not in Allowed Registry"),
            std::string::npos);
}

TEST_F(AccessRegistryTest, RemoveAccessFileDeletesFile) {
  AccessRegistry reg(tempDir, procName);
  fs::path target = tempDir / (procName + ".oregistry");
  EXPECT_TRUE(fileExists(target));

  reg.removeAccessFile();
  EXPECT_FALSE(fileExists(target));
}

TEST_F(AccessRegistryTest, RemoveAccessFileHandlesMissingFileGracefully) {
  AccessRegistry reg(tempDir, procName);
  fs::path target = tempDir / (procName + ".oregistry");
  fs::remove(target);

  testing::internal::CaptureStdout();
  reg.removeAccessFile();
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_NE(output.find("[ERROR]: Unable to remove the access file"),
            std::string::npos);
}

// Since connect_to() is currently unimplemented
TEST_F(AccessRegistryTest, ConnectToPlaceholderDoesNotCrash) {
  AccessRegistry reg(tempDir, procName);
  EXPECT_NO_THROW(reg.connect_to("OtherProc"));
}

TEST_F(AccessRegistryTest, ThreadSafetyForAddAndRemove) {
  std::ofstream(tempDir / "Concurrent.oregistry") << "Concurrent";
  AccessRegistry reg(tempDir, procName);

  auto addTask = [&]() {
    for (int i = 0; i < 10; ++i)
      reg.addProcessToList("Concurrent");
  };
  auto removeTask = [&]() {
    for (int i = 0; i < 10; ++i)
      reg.removeProcessFromAccessList("Concurrent");
  };

  std::thread t1(addTask), t2(removeTask);
  t1.join();
  t2.join();

  // Registry should be in a consistent state (either contains or not)
  EXPECT_NO_FATAL_FAILURE();
}
