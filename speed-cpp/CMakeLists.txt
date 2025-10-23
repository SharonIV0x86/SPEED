cmake_minimum_required(VERSION 3.14)
project(AccessRegistryTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# Include header files
include_directories(${CMAKE_SOURCE_DIR}/include)

# Fetch GoogleTest automatically
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()
# Find libsodium
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)

# Include dirs
include_directories(${SODIUM_INCLUDE_DIRS})
link_directories(${SODIUM_LIBRARY_DIRS})

# --- Add your test executable ---
add_executable(AccessRegistry_test
    tests/AccessRegistry_Test.cpp   
    tests/BinaryManager_Test.cpp   
    tests/per_sender_fifo_mock_Test.cpp
    src/AccessRegistry.cpp
    src/Utils.cpp
)

target_link_libraries(AccessRegistry_test
    GTest::gtest_main
    pthread
    sodium
)

include(GoogleTest)
gtest_discover_tests(AccessRegistry_test)
