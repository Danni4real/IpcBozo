//
// Created by dan on 24-3-14.
//

#ifndef SERVER_2_API_H
#define SERVER_2_API_H

namespace IpcBozoServer_2 {
static constexpr char IpcChannel[] = "channel_2";

enum {
    Method1, // bool method1();
    Method2, // long method2(bool arg1);
    Method3, // void method3(double arg1, std::string arg2);
    Method4  // not supported yet
};
enum {
    Signal1 // void signal1_handler();
};
}

#endif //SERVER_2_API_H
