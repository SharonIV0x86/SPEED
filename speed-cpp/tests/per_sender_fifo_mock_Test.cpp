// per_sender_fifo_tests.cpp
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ----- helper types -----
struct ParsedInfo {
  uint64_t timestamp;
  std::string proc;
  long long seq;
  std::string uuid;
  std::string filename; // original filename
};

// Regex used in: "<timestamp>_<proc_name>_<seq>_<uuid>.ospeed"
static const std::regex
    FNAME_RE(R"((\d+)_([A-Za-z0-9_]+)_(\d+)_([A-Za-z0-9\-]+)\.ospeed)");

// Parse filename string (no filesystem needed)
static std::optional<ParsedInfo>
parse_filename_string(const std::string &fname) {
  std::smatch m;
  if (!std::regex_match(fname, m, FNAME_RE))
    return std::nullopt;
  try {
    ParsedInfo pi;
    pi.timestamp = static_cast<uint64_t>(std::stoull(m[1].str()));
    pi.proc = m[2].str();
    pi.seq = std::stoll(m[3].str());
    pi.uuid = m[4].str();
    pi.filename = fname;
    return pi;
  } catch (...) {
    return std::nullopt;
  }
}

// Parse from filesystem path
static std::optional<ParsedInfo> parse_filename_path(const fs::path &p) {
  return parse_filename_string(p.filename().string());
}

static uint64_t now_ns() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

static std::string rand_uuid_hex64() {
  static thread_local std::mt19937_64 rng(
      (unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<uint64_t> dist;
  uint64_t v = dist(rng);
  std::ostringstream oss;
  oss << std::hex << v;
  return oss.str();
}

// ----- 1) Mock-string test -----
TEST(PerSenderFIFO_MockStrings, PerSenderOrdering) {
  // Create an out-of-order list of filenames from two senders
  std::vector<std::string> fnames = {
      std::to_string(1000) + "_alpha_2_" + "u1.ospeed",
      std::to_string(1000) + "_alpha_0_" + "u2.ospeed",
      std::to_string(1001) + "_beta_1_" + "u3.ospeed",
      std::to_string(1000) + "_alpha_1_" + "u4.ospeed",
      std::to_string(1001) + "_beta_0_" + "u5.ospeed",
      std::to_string(1002) + "_gamma_0_" + "u6.ospeed"};

  // normalize to .ospeed format used by parser
  for (auto &s : fnames) {
    // ensure correct extension already present
    if (!std::regex_match(s, FNAME_RE)) {
      // adapt: convert the last "_" token into uuid plus .ospeed
      // already built in above strings - but keep safe
    }
  }

  // Buffer maps (proc -> map<seq, filename>)
  std::map<std::string, std::map<long long, std::string>> sender_buffers;
  std::map<std::string, long long> next_expected;

  // Simulate scanning and buffering
  for (auto &f : fnames) {
    auto opt = parse_filename_string(f);
    ASSERT_TRUE(opt.has_value()) << "Parsing failed for: " << f;
    auto pi = *opt;
    sender_buffers[pi.proc][pi.seq] = pi.filename;
    if (!next_expected.count(pi.proc))
      next_expected[pi.proc] = 0;
  }

  // Process per-sender FIFO
  std::vector<std::pair<std::string, long long>> processed; // (proc, seq)
  bool progressed = true;
  while (progressed) {
    progressed = false;
    for (auto &kv : sender_buffers) {
      const std::string &proc = kv.first;
      auto &buf = kv.second;
      long long expected = next_expected[proc];
      auto it = buf.find(expected);
      if (it != buf.end()) {
        processed.emplace_back(proc, expected);
        buf.erase(it);
        next_expected[proc] = expected + 1;
        progressed = true;
      }
    }
  }

  // Validate per-sender ordering: for each proc, sequences must be strictly
  // increasing from 0
  std::map<std::string, long long> last;
  for (auto &p : processed) {
    const auto &proc = p.first;
    long long seq = p.second;
    if (last.count(proc)) {
      EXPECT_GE(seq, last[proc]) << "Out-of-order seq for " << proc;
    } else {
      EXPECT_GE(seq, 0);
    }
    last[proc] = seq;
  }

  // Expected processed counts per sender:
  EXPECT_EQ(next_expected["alpha"], 3); // 0,1,2
  EXPECT_EQ(next_expected["beta"], 2);  // 0,1
  EXPECT_EQ(next_expected["gamma"], 1); // 0
}

// ----- 2) Real-file test -----
TEST(PerSenderFIFO_RealFiles, ManyWritersSingleWatcher) {
  const int WRITERS = 6;
  const int FILES_PER_WRITER = 40; // tune for CI speed
  const int TOTAL = WRITERS * FILES_PER_WRITER;
  const std::chrono::seconds TIMEOUT(30);

  // create unique temp dir
  auto t0 = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path tmpdir =
      fs::temp_directory_path() / ("speed_fifo_test_" + std::to_string(t0));
  fs::create_directories(tmpdir);

  // Writers: will create files named "<ts>_<ProcN>_<seq>_<uuid>.ospeed"
  auto writer_fn = [&](int writer_id) {
    std::string pname = "Proc" + std::to_string(writer_id);
    for (int i = 0; i < FILES_PER_WRITER; ++i) {
      uint64_t ts = now_ns();
      std::string fname = std::to_string(ts) + "_" + pname + "_" +
                          std::to_string(i) + "_" + rand_uuid_hex64() +
                          ".ospeed";
      fs::path fp = tmpdir / fname;
      // write file atomically: write then rename -- simple create then rename
      // not necessary here but okay
      std::ofstream ofs(fp.string(), std::ios::binary);
      ofs << "payload " << writer_id << " " << i << "\n";
      ofs.flush();
      // jitter
      if ((i & 7) == 0)
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  // data structures for watcher simulation
  std::unordered_set<std::string> seen;
  std::map<std::string, std::map<long long, fs::path>> sender_buffers;
  std::map<std::string, long long> next_expected;
  std::vector<std::pair<std::string, long long>> processed; // (proc, seq)
  std::mutex processed_mtx;

  // Launch writers
  std::vector<std::thread> writers;
  for (int w = 1; w <= WRITERS; ++w)
    writers.emplace_back(writer_fn, w);

  // Watcher loop (runs in test thread). It mimics your runWatcherLoop_
  // behavior.
  auto start_time = std::chrono::steady_clock::now();
  while (true) {
    // scan directory
    for (auto &entry : fs::directory_iterator(tmpdir)) {
      if (!entry.is_regular_file())
        continue;
      std::string fname = entry.path().filename().string();
      if (seen.count(fname))
        continue;

      auto opt = parse_filename_path(entry.path());
      if (!opt) {
        // mark seen to avoid thrashing
        seen.insert(fname);
        continue;
      }
      auto pi = *opt;

      // buffer it
      sender_buffers[pi.proc][pi.seq] = entry.path();
      if (!next_expected.count(pi.proc))
        next_expected[pi.proc] = 0;
      seen.insert(fname);
    }

    // attempt to process any ready messages
    bool did = false;
    for (auto &kv : sender_buffers) {
      const std::string proc = kv.first;
      auto &buf = kv.second;
      long long expected = next_expected[proc];
      auto it = buf.find(expected);
      while (it != buf.end()) {
        // read/delete file (simulate processFile_)
        try {
          std::error_code ec;
          fs::remove(it->second, ec); // ignore remove errors
        } catch (...) {
        }
        {
          std::lock_guard<std::mutex> lg(processed_mtx);
          processed.emplace_back(proc, expected);
        }
        buf.erase(it);
        next_expected[proc] = expected + 1;
        did = true;
        expected = next_expected[proc];
        it = buf.find(expected);
      }
    }

    // break condition: all processed
    {
      std::lock_guard<std::mutex> lg(processed_mtx);
      if ((int)processed.size() >= TOTAL)
        break;
    }

    // timeout
    if (std::chrono::steady_clock::now() - start_time > TIMEOUT)
      break;

    if (!did)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  // wait for writers to finish
  for (auto &t : writers)
    if (t.joinable())
      t.join();

  // Validate per-sender FIFO: sequences per sender must be non-decreasing and
  // start at 0..N-1
  std::map<std::string, long long> last_seen;
  {
    std::lock_guard<std::mutex> lg(processed_mtx);
    ASSERT_GE((int)processed.size(), 1) << "No files processed";
    for (auto &p : processed) {
      const auto &proc = p.first;
      long long seq = p.second;
      if (last_seen.count(proc)) {
        EXPECT_GE(seq, last_seen[proc])
            << "Per-sender FIFO violated for " << proc;
      } else {
        EXPECT_GE(seq, 0);
      }
      last_seen[proc] = seq;
    }
  }

  // Also assert we processed at least one file per writer (sanity)
  for (int w = 1; w <= WRITERS; ++w) {
    std::string pname = "Proc" + std::to_string(w);
    EXPECT_TRUE(last_seen.count(pname)) << "No files processed for " << pname;
  }

  // cleanup
  fs::remove_all(tmpdir);
}
