# SPEED
Secure Process-To-Process Encrypted Exchange and Delivery.


## Work In Progress
Currently we are focusing on the C++ binding for the MVP and then we will move to Java.

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