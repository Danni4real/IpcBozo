//
// Created by dan on 24-3-14.
//

#ifndef SERVER_1_API_H
#define SERVER_1_API_H

namespace IpcBozoServer_1 {
static constexpr char IpcChannel[] = "channel_1";

enum {
    Method1 // void method1();
};
enum {
    Signal1, // void signal1_handler();
    Signal2  // void signal2_handler(bool arg1, long arg2, double arg3, std::string arg4);
};
}

#endif //SERVER_1_API_H
