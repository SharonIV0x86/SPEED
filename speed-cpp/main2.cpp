#include <iostream>
#include "include/SPEED.hpp"

void user_callback(const SPEED::PMessage& msg){
    std::cout << "\n\nRecieved message from " << msg.sender_name << "\n";
    std::cout << "Message: " << msg.message << "\n";
    std::cout << "TS: " << msg.timestamp <<"\n";
}

int main(){
    SPEED::SPEED spd("Riddhi", SPEED::ThreadMode::Multi);
    spd.setCallback(user_callback);
    spd.addProcess("Anirudh");
    spd.setKeyFile("/home/jasper/Development/SPEED/speed-cpp/config.key");
    spd.start();
    while (true) {
        std::string s;
        std::cout << "Enter a message to send: " << std::flush;
        std::getline(std::cin, s);   // use getline now
        if (!s.empty()){
            if(s == "--exit"){
                break;
            }
            spd.sendMessage(s, "Anirudh");
        }
    }
    
}