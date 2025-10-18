#include <iostream>
#include "include/SPEED.hpp"

void user_callback(const SPEED::PMessage& msg){
    std::cout << "\n\nRecieved message from " << msg.sender_name << "\n";
    std::cout << "Message: " << msg.message << "\n";
    std::cout << "TS: " << msg.timestamp <<"\n";
}

int main(){
    SPEED::SPEED spd("Proc_2", SPEED::ThreadMode::Multi);
    spd.setCallback(user_callback);
    spd.addProcess("Proc_1");
    spd.setKeyFile("/home/jasper/Development/SPEED/speed-cpp/config.key");
    spd.start();
    while (true) {
        std::string s;
        std::cout << "Enter a message to send: " << std::flush;
        std::getline(std::cin, s);   // use getline now
        if (!s.empty()){
            if(s == "--exit"){
                break;
            }else if(s == "--ping"){
                spd.ping("Proc_1");
            }else if(s == "--pong"){
                spd.pong("Proc_1");
            }else if(s == "--kill"){
                spd.kill();
            }
            else if(s == "--getGR"){
                spd.printGlobalRegistry_();
            }
            else if(s == "--getAL"){
                spd.printAccessList_();
            }
            else if(s == "--getCL"){
                spd.printConnectedList_();
            }
            else{
                spd.sendMessage(s, "Proc_1");
            }
        }
    }
    
}