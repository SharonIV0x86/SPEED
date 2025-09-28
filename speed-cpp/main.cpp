#include <iostream>
#include <thread>
#include <chrono>
#include "include/SPEED.hpp"

void user_callback(const SPEED::PMessage& msg){
    std::cout << "Message: " << msg.message << "\n";
    std::cout << "Sender: " << msg.sender_name << "\n";
    std::cout << "TS: " << msg.timestamp << "\n";
}

int main(){
    std::cout << "Hello world\n";
    SPEED::SPEED spd("Jaspers", SPEED::ThreadMode::Multi);
    spd.setCallback(user_callback);
    // spd.trigger();
    spd.addProcess("Lemon");
    spd.setKeyFile("/home/jasper/Development/SPEED/speed-cpp/config.key");
    spd.start();
    spd.sendMessage("I AM A LITTLE DEMON");
    spd.sendMessage("I AM A LITTLE JASPER");
    spd.sendMessage("I AM A LITTLE LEMNON");
    // spd.mockWrite();
    // SPEED::Message msg      = spd.mockRead();

    // std::string version     = std::to_string(msg.header.version);
    // SPEED::MessageType type = msg.header.type;
    // std::string sender_pid  = std::to_string(msg.header.sender_pid);
    // std::string timestamp   = std::to_string(msg.header.timestamp);    
    // std::string seq_num     = std::to_string(msg.header.seq_num);
    // std::string sender      = msg.header.sender;    
    // std::string reciever    = msg.header.reciever;    
    // std::string payload     = std::string(msg.payload.begin(), msg.payload.end());
    
    // std::cout << "\n\nRead Object Version: "    << version      << "\n";
    // std::cout << "Read Object Type: "           << (int)type    << "\n";
    // std::cout << "Read Object sender_pid: "     << sender_pid   << "\n";
    // std::cout << "Read Object timestamp: "      << timestamp    << "\n";
    // std::cout << "Read Object seq_num: "        << seq_num      << "\n";
    // std::cout << "Read Object sender: "         << sender       << "\n";
    // std::cout << "Read Object reciever: "       << reciever     << "\n";
    // std::cout << "Read Object Payload:"         << payload      << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

}
