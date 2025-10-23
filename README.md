# SPEED
Secure Process-To-Process Encrypted Exchange and Delivery.
A lightweight, file-based IPC library for secure local process communication. SPEED enables encrypted message passing between applications using a simple file system approach, making it ideal for embedded systems, restricted environments, and cross-platform tools.

## Work In Progress
The current focus is the C++ binding (MVP). Java and additional language bindings are planned.


## Prerequisites
- C++17 capable compiler (g++, clang, MSVC)
- [libsodium](https://libsodium.gitbook.io/doc/) installed on the system

### To compile C++ Code.
Move to ``speed-cpp`` folder.
```sh
cd speed-cpp
```
Compile the ``main.cpp`` and run the ``app`` binary.
```sh
g++ -Iinclude main.cpp src/*.cpp -o app
```
If you want to contribute make sure the code is clang formatted using.
```sh
clang-format -i src/* && clang-format -i include/*
```

## Minimal Usage Example
```cpp
#include "include/SPEED.hpp"

void messageCallback(const SPEED::PMessage& msg) {
    std::cout << "Received from " << msg.sender_name << ": " << msg.message << std::endl;
}

int main() {
    SPEED::SPEED ipc("MyProcess", SPEED::ThreadMode::Multi);
    ipc.setCallback(messageCallback);
    ipc.addProcess("OtherProcess");
    ipc.setKeyFile("config.key");
    ipc.start();
    
    ipc.sendMessage("Hello World", "OtherProcess");
}
```


### Protocol Documentation [here](docs/SPEED_Protocol_doc.md)

## Why Choose SPEED?
### Ideal For:
- **Embedded systems** with limited IPC options
- **Network-restricted environments** where sockets are blocked
- **Cross-platform tools** needing consistent behavior on Windows, Linux, macOS
- **Educational projects** learning about inter-process communication
- **Quick prototypes** where simplicity trumps performance
- **Zero-dependency requirements** - only needs libsodium

## Key Features:
- **File-Based IPC**: Uses standard filesystem primitives — works even on restricted or offline systems.
- **Encryption Built-In**: All messages can be encrypted via libsodium
- **Cross-Process Messaging**: Will work across languages - Python, C++, Rust, Go, etc.
- **Zero Daemons**, Zero Networking: No sockets, no servers — just local secure communication.
- **Minimal Dependencies**: Only needs libsodium for encryption.
- **Cross-Platform**: Runs on Windows, Linux, macOS, and embedded environments.
## Why SPEED Exists

SPEED was born from a simple need: straightforward inter-process communication that works reliably across all platforms, even in restricted environments.
SPEED was never meant to compete with high-performance IPC systems like sockets, pipes, or shared memory. It is more focused towards small use cases and projects.

Sometimes, the right tool isn’t the fastest or most sophisticated. It’s the one that actually works for your situation. SPEED is that simple, reliable choice for developers who just need processes to communicate without the usual complexity.
### SPEED exists for:
- Simplicity: Minimal and simple to use with clean API and access control.
- Security: Authenticated encryption powered by libsodium.
- Accessibility: Works in restricted or sandboxed environments.
- Portability: Runs on virtually any operating system.
### SPEED is not:
- A production-grade IPC framework
- A high-throughput or real-time message bus

# SPEED RFI (Remote Function Invocation)
SPEED RFI enables lightweight, cross-process function calls between connected processes using the SPEED IPC layer.
For example, if Process P1 and Process P2 are connected through SPEED, then P1 can remotely invoke functions registered by P2.

```cpp
void add(const std::vector<std::string>& args) {
    int a = std::stoi(args[0]);
    int b = std::stoi(args[1]);
    std::cout << "Sum: " << (a + b) << "\n";
}

ipc_P2.registerMethod("add", add);
```
A process can register functions that can be invoked remotely using the registerMethod() API. Only registered functions are eligible for remote invocation. Now **P1** wants to invoke this method in **P2**.

To invoke a registered function from another process, use the ``invokeMethod()`` API:
```cpp

ipc_P1.invokeMethod("add", "P1", {"10", "10"});
```
| Parameter      | Type                       | Description                                           |
| -------------- | -------------------------- | ----------------------------------------------------- |
| `method_name`  | `std::string`              | Name of the function to invoke.                       |
| `process_name` | `std::string`              | Target process name where the function is registered. |
| `args`         | `std::vector<std::string>` | List of arguments to pass to the remote function.     |

### Notes and Conventions
- All arguments must be passed as strings.
- The receiving function is responsible for casting and parsing parameters into the required types.
- Only registered functions can be invoked remotely.
- If the target process or function name does not exist, SPEED will safely log an error.
- RFI is fully asynchronous - invocation requests are queued and delivered through SPEED’s internal message-passing system.
