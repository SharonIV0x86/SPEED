#include <iostream>
#include <thread>
#include <chrono>
#include "include/SPEED.hpp"

void user_callback(const SPEED::PMessage& msg){
    std::cout << "\n\nMessage: " << msg.message << "\n";
    std::cout << "Sender: " << msg.sender_name << "\n";
    std::cout << "TS: " << msg.timestamp <<"\n";

}

int main(){
    // std::cout << "Hello world\n";
    SPEED::SPEED spd("Proc_1", SPEED::ThreadMode::Multi);
    spd.setCallback(user_callback);
    spd.addProcess("Proc_2");
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
            else if(s == "--ping"){
                spd.ping("Proc_2");
            }
            else if(s == "--pong"){
                spd.pong("Proc_2");
            }
            else if(s == "--kill"){
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
                spd.sendMessage(s, "Proc_2");
            }
        }
    }

    std::cout << "[INFO]: Killing the process\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1200000));

}
