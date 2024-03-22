
#include "IpcBozo/bozo.h"
#include "server_1_api.h"
#include "server_2_api.h"

double timing(bool start_as_true) {
  static std::chrono::time_point<std::chrono::system_clock> start;
  static std::chrono::time_point<std::chrono::system_clock> end;

  if (start_as_true) {
    start = std::chrono::system_clock::now();
    return 0;
  } else {
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    return elapsed_seconds.count();
  }
}

void server_2_signal1_handler() {
  std::cout << "server_2 signal1 received" << std::endl;
}

void server_1_signal1_handler() {
  std::cout << "server_1 signal1 received" << std::endl;
}

void server_1_signal2_handler(bool arg1, long arg2, double arg3, std::string arg4) {
  std::cout << "server_1 signal2(" << arg1 << ','
            << arg2 << ','
            << arg3 << ','
            << arg4 << ") received"
            << std::endl;
}

int main() {
  IpcBozo client_1(IpcBozoServer_1::IpcName);
  IpcBozo client_2(IpcBozoServer_2::IpcName);

  if (!client_1.client_init()) { return 1; }
  if (!client_2.client_init()) { return 1; }

  client_1.register_signal_handler(IpcBozoServer_1::Signal1, server_1_signal1_handler);
  client_1.register_signal_handler(IpcBozoServer_1::Signal2, server_1_signal2_handler);
  client_2.register_signal_handler(IpcBozoServer_2::Signal1, server_2_signal1_handler);

  if (client_1.remote_call<IpcBozoServer_1::Method1>()) {
    std::cout << "server_1: method1 return " << std::endl;
  }

  bool ret_bool;
  if (client_2.remote_call<bool, IpcBozoServer_2::Method1>(&ret_bool)) {
    std::cout << "server_2: method1 return: " << ret_bool << std::endl;
  }

  long ret_long;
  if (client_2.remote_call<long, IpcBozoServer_2::Method2, bool>(&ret_long, true)) {
    std::cout << "server_2: method2 return: " << ret_long << std::endl;
  }

  if (client_2.remote_call<IpcBozoServer_2::Method3, double, std::string>(0.001,"bozo")) {
    std::cout << "server_2: method3 return" << std::endl;
  }

  // invalid call
  if (client_2.remote_call<IpcBozoServer_2::Method4>()) {
    std::cout << "server_2: method4 return" << std::endl;
  }

  IpcBozo::client_loop_run();
  return 0;
}