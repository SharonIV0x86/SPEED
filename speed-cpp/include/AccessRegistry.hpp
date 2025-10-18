#pragma once
#include "Utils.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>
namespace SPEED {
class AccessRegistry {
public:
  AccessRegistry(const std::filesystem::path &, const std::string &);

  void addProcessToList(const std::string &proc_name);
  void incrementalBuildGlobalRegistry();
  void removeAccessFile();
  void syncAccessRegistry();

  bool removeProcessFromGlobalRegistry(const std::string &proc_name);
  bool checkAccess(const std::string &proc_name) const;
  bool removeProcessFromAccessList(const std::string &proc_name);
  bool removeProcessFromConnectedList(const std::string &proc_name);
  bool connect_to(const std::string &);
  bool checkGlobalRegistry(const std::string &proc_name) const;
  bool check_connection(const std::string &proc_name) const;

  const std::filesystem::path &getAccessRegistryPath() const;
  const std::unordered_set<std::string> getGlobalRegistry() const;
  const std::unordered_set<std::string> getAccessList() const;
  const std::unordered_set<std::string> getConnectedList() const;

private:
  void putAccessFile();
  void printRegistry() const;
  void try_connect_all();
  std::unordered_set<std::string> allowedProcesses_;
  std::unordered_set<std::string> global_registry_;
  std::unordered_set<std::string> connected_list_;
  std::filesystem::path ac_path_;
  std::string access_filename_;
  std::string proc_name_;

  mutable std::mutex mtx_; // protects shared state
};

} // namespace SPEED
