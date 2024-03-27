
#include <thread>
#include "IpcBozo/bozo.h"
#include "server_1_api.h"

IpcBozo ipc_server(IpcBozoServer_1::IpcChannel);

void method1() {
  std::cout << "server_1: some one call method1()" << std::endl;

  ipc_server.send_signal<IpcBozoServer_1::Signal1>();
  std::cout << "server_1: send signal1" << std::endl;

  ipc_server.send_signal<IpcBozoServer_1::Signal2, bool, long, double, std::string>(false, -1, 0.1, "ban bozo");
  std::cout << "server_1: send signal2" << std::endl;
}

int main() {

  if (!ipc_server.server_init()) {
    return 1;
  }

  ipc_server.register_method(IpcBozoServer_1::Method1, method1);
  ipc_server.server_loop_run();

  return 0;
}