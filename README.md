# IpcBozo

A C++ IPC library based on Dbus with a very "bozo" interface

Dependency:
1. glib-2.0 
2. gio-2.0 
3. dbus-1 
4. dbus-glib-1
5. C++ 17
6. nlohmann json (already included)

Limitations:
1. only support char, bool, int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, std::string as method/signal argument type or return type;

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
      if (client.remote_call<bool, Method_1, double, std::string>(&ret, 0.01, "bozo")) {
        std::cout << "ipc_channel_1: Method_1 return: " << ret << std::endl;
      }

      IpcBozo::client_loop_run();
      return 0;
    }
    
