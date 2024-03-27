# IpcBozo

A C++ IPC library based on Dbus with a very "bozo" interface

Dependency:
1. C++17
2. nlohmann json (already included)
3. libglib2.0-dev
4. libdbus-1-dev 
5. libdbus-glib-1-dev

Limitations:
1. only support char, bool, int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, std::string as method/signal argument type or method return type;

Example:

  server.cpp:
  
    #include "IpcBozo/bozo.h"
    enum {Method_1};
    
    bool method_1(double arg1, std::string arg2) {
      std::cout << "a bozo called method_1(" << arg1 << ',' << arg2 << ')' << std::endl;
      return true;
    }
    
    int main() {
      IpcBozo server("ipc_channel_1");
      
      if (!server.server_init()) { return 1; }

      server.register_method(Method_1, method_1);
      server.server_loop_run();
      return 0;
    }

  client.cpp:
  
    #include "IpcBozo/bozo.h"
    enum {Method_1}; // same as server to be able to communicate
    
    int main() {
      IpcBozo client("ipc_channel_1"); // same as server to be able to communicate

      if (!client.client_init()) { return 1; }

      bool ret;
      if (client.remote_call<Method_1>(&ret, 0.01, "bozo")) {
        std::cout << "ipc_channel_1: Method_1 return: " << ret << std::endl;
      }

      IpcBozo::client_loop_run();
      return 0;
    }
    
