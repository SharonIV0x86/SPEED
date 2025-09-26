#include <iostream>
#include "include/SPEED.hpp"

void user_callback(const SPEED::PMessage& msg){
    std::cout << "Message: " << msg.message << "\n";
    std::cout << "Sender: " << msg.sender_name << "\n";
    std::cout << "TS: " << msg.timestamp << "\n";
}

int main(){
    std::cout << "Hello world\n";
    SPEED::SPEED spd("Jasper's", SPEED::ThreadMode::Multi);
    spd.setCallback(user_callback);
    spd.trigger();
    spd.addProcess("Lemon");
}
