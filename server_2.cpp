
#include <thread>
#include "IpcBozo/bozo.h"
#include "server_2_api.h"

IpcBozo ipc_server(IpcBozoServer_2::IpcChannel);

bool method1() {
  std::cout << "server_2: some one call method1()" << std::endl;

  ipc_server.send_signal<IpcBozoServer_2::Signal1>();
  std::cout << "server_2: send signal1" << std::endl;

  return false;
}

long method2(bool arg1) {
  std::cout << "server_2: some one call method2("
            << arg1 << ')'
            << std::endl;
  return 12345678;
}

void method3(double arg1, std::string arg2) {
  std::cout << "server_2: some one call method3("
            << arg1 << ','
            << arg2 << ')'
            << std::endl;
}

int main() {

  if (!ipc_server.server_init()) {
    return 1;
  }

  ipc_server.register_method(IpcBozoServer_2::Method1, method1);
  ipc_server.register_method(IpcBozoServer_2::Method2, method2);
  ipc_server.register_method(IpcBozoServer_2::Method3, method3);
  ipc_server.server_loop_run();

  return 0;
}