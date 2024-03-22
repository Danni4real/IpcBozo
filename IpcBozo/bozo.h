//
// Created by dan on 24-3-11.
//

#ifndef IPCBOZO_BOZO_H
#define IPCBOZO_BOZO_H

#include <string>
#include <vector>
#include <iostream>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gprintf.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "json.hpp"

class IpcBozo {
 public:

  // client and server should set same ipc_name to communicate
  IpcBozo(const std::string &ipc_name) {
    dbus_server_name = "com.ipc_bozo_" + ipc_name + ".server";
    dbus_object_name = "/com/ipc_bozo_" + ipc_name + "/object";
    dbus_interface_name = "com.ipc_bozo_" + ipc_name + ".interface";
  }

  bool client_init();

  bool server_init();

  void server_loop_run();

  static void client_loop_run();

  template<typename R, typename... Args>
  void register_method(int method_id, R(*func)(Args...)) {
    auto f = [=](const std::vector<std::string> &arg_vec) {
      return toString(std::apply(func, toTuple<Args...>(arg_vec)));
    };
    method_map[method_id] = f;
  }

  template<typename... Args>
  void register_method(int method_id, void(*func)(Args...)) {
    auto f = [=](const std::vector<std::string> &arg_vec) {
      std::apply(func, toTuple<Args...>(arg_vec));
    };
    method_map_no_ret[method_id] = f;
  }

  template<typename R>
  void register_method(int method_id, R(*func)()) {
    auto f = [=]() {
      return toString(func());
    };
    method_map_no_arg[method_id] = f;
  }

  void register_method(int method_id, void(*func)()) {
    auto f = [=]() {
      func();
    };
    method_map_no_ret_no_arg[method_id] = f;
  }

  template<typename... Args>
  void register_signal_handler(int signal_id, void(*func)(Args...)) {
    auto f = [=](const std::vector<std::string> &arg_vec) {
      std::apply(func, toTuple<Args...>(arg_vec));
    };
    signal_handler_map[signal_id] = f;
  }

  void register_signal_handler(int signal_id, void(*func)()) {
    auto f = [=]() {
      func();
    };
    signal_handler_map_no_arg[signal_id] = f;
  }

  template<typename T, int MethodId, typename... Ts>
  bool remote_call(T *out_arg, Ts... in_args) {
    std::string call_ret;
    auto call_succeed = dbus_call(serialize(MethodId, in_args...), &call_ret);
    *out_arg = stringTo<T>(call_ret);

    return call_succeed;
  }

  template<int MethodId>
  bool remote_call() {
    return dbus_call(serialize(MethodId), nullptr);
  }

  template<int MethodId, typename... Ts>
  bool remote_call(Ts... in_args) {
    return dbus_call(serialize(MethodId, in_args...), nullptr);
  }

  template<typename T, int MethodId>
  bool remote_call(T *out_arg) {
    std::string call_ret;
    auto call_succeed = dbus_call(serialize(MethodId), &call_ret);
    *out_arg = stringTo<T>(call_ret);

    return call_succeed;
  }

  template<int SignalId, typename... Ts>
  void send_signal(Ts... in_args) {
    dbus_send_signal(serialize(SignalId, in_args...));
  }

  template<int SignalId>
  void send_signal() {
    dbus_send_signal(serialize(SignalId));
  }

 private:

  static constexpr char MethodName[] = "method";
  static constexpr char SignalName[] = "signal";
  static constexpr char MethodSignalIdKey[] = "method_signal_id";
  static constexpr char ArgVecKey[] = "arg_vec";
  static constexpr char True[] = "true";
  static constexpr char False[] = "false";

  std::string dbus_server_name;
  std::string dbus_object_name;
  std::string dbus_interface_name;

  DBusConnection *server_dbus_connection = nullptr;
  GDBusProxy *client_dbus_proxy = nullptr;

  std::map<int, std::function<void(void)>> signal_handler_map_no_arg;
  std::map<int, std::function<void(const std::vector<std::string> &arg_vec)>> signal_handler_map;
  std::map<int, std::function<void(void)>> method_map_no_ret_no_arg;
  std::map<int, std::function<std::string(void)>> method_map_no_arg;
  std::map<int, std::function<void(const std::vector<std::string> &arg_vec)>> method_map_no_ret;
  std::map<int, std::function<std::string(const std::vector<std::string> &arg_vec)>> method_map;

  DBusObjectPathVTable dbus_server_v_table = {
    .message_function = server_message_handler
  };

  static int get_method_signal_id(const std::string &arg);

  static std::vector<std::string> get_arg_vec(const std::string &arg);

  static void
  on_handle_signal(GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *parameters,
                   gpointer object);

  static DBusHandlerResult server_message_handler(DBusConnection *conn, DBusMessage *message, void *object);

  bool dbus_call(const std::string &in_arg, std::string *out_arg);

  void dbus_send_signal(const std::string &arg);

  bool signal_handler_registered(int signal_id);

  bool call_method(const std::string &arg, std::string *ret);

  template<typename T>
  void assert_valid_type(T) {
    static_assert(std::is_same_v<T, char> ||
                  std::is_same_v<T, bool> ||
                  std::is_same_v<T, std::string> ||
                  std::is_same_v<T, int> ||
                  std::is_same_v<T, unsigned> ||
                  std::is_same_v<T, long> ||
                  std::is_same_v<T, unsigned long> ||
                  std::is_same_v<T, long long> ||
                  std::is_same_v<T, unsigned long long> ||
                  std::is_same_v<T, float> ||
                  std::is_same_v<T, double> ||
                  std::is_same_v<T, long double>);
  }

  template<typename T>
  std::string toString(T arg) {
    assert_valid_type(arg);

    if constexpr (std::is_same_v<T, char>) {
      return std::string(1, arg);
    } else if constexpr (std::is_same_v<T, bool>) {
      return arg ? True : False;
    } else if constexpr (std::is_same_v<T, std::string>) {
      return arg;
    } else {
      return std::to_string(arg);;
    }
  }

  template<typename T>
  T stringTo(const std::string &arg) {
    assert_valid_type(T{});

    if constexpr (std::is_same_v<T, char>) {
      return arg[0];
    } else if constexpr (std::is_same_v<T, bool>) {
      if (arg == True) { return true; }
      return false;
    } else if constexpr (std::is_same_v<T, std::string>) { return arg; }
    else if constexpr (std::is_same_v<T, int>) { return std::stoi(arg); }
    else if constexpr (std::is_same_v<T, unsigned>) { return (unsigned) std::stoul(arg); }
    else if constexpr (std::is_same_v<T, long>) { return std::stol(arg); }
    else if constexpr (std::is_same_v<T, unsigned long>) { return std::stoul(arg); }
    else if constexpr (std::is_same_v<T, long long>) { return std::stoll(arg); }
    else if constexpr (std::is_same_v<T, unsigned long long>) { return std::stoull(arg); }
    else if constexpr (std::is_same_v<T, float>) { return std::stof(arg); }
    else if constexpr (std::is_same_v<T, double>) { return std::stod(arg); }
    else if constexpr (std::is_same_v<T, long double>) { return std::stold(arg); }
    else { assert(false && "unsupported method argument or return type!"); /*should never get here*/}
  }

  template<typename T>
  void toVector(std::vector<std::string> *arg_vec, T arg) {
    arg_vec->push_back(toString(arg));
  }

  template<typename T, typename... Ts>
  void toVector(std::vector<std::string> *arg_vec, T arg1, Ts... args) {
    toVector(arg_vec, arg1);
    if (sizeof...(args) > 0) {
      toVector(arg_vec, args...);
    }
  }

  template<typename... Ts>
  std::string serialize(int method_signal_id, Ts... args) {
    std::vector<std::string> arg_vec;
    toVector(&arg_vec, args...);

    nlohmann::json j;
    j[MethodSignalIdKey] = method_signal_id;
    j[ArgVecKey] = arg_vec;

    return to_string(j);
  }

  static std::string serialize(int method_signal_id) {
    nlohmann::json j;
    j[MethodSignalIdKey] = method_signal_id;

    return to_string(j);
  }

  template<typename... Ts, std::size_t... Is>
  std::tuple<Ts...> toTupleImpl(const std::vector<std::string> &arg_vec, std::index_sequence<Is...>) {
    return {stringTo<Ts>(arg_vec[Is])...};
  }

  template<typename... Ts>
  std::tuple<Ts...> toTuple(const std::vector<std::string> &arg_vec) {
    return toTupleImpl<Ts...>(arg_vec, std::index_sequence_for<Ts...>{});
  }
};

#endif //IPCBOZO_BOZO_H
