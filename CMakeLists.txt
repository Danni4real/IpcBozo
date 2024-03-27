cmake_minimum_required(VERSION 3.22)
project(IpcBozo)

set(CMAKE_CXX_STANDARD 17)

INCLUDE_DIRECTORIES(/usr/include/dbus-1.0/
                    /usr/include/glib-2.0/
                    /usr/include/gio-unix-2.0/
                    /usr/lib/x86_64-linux-gnu/dbus-1.0/include/
                    /usr/lib/x86_64-linux-gnu/glib-2.0/include/)

add_executable(server1 server_1.cpp
        IpcBozo/bozo.cpp)
target_link_libraries(server1 glib-2.0 gio-2.0 dbus-1 dbus-glib-1)

add_executable(server2 server_2.cpp
        IpcBozo/bozo.cpp)
target_link_libraries(server2 glib-2.0 gio-2.0 dbus-1 dbus-glib-1)

add_executable(client client.cpp
        IpcBozo/bozo.cpp)
target_link_libraries(client glib-2.0 gio-2.0 dbus-1 dbus-glib-1)
