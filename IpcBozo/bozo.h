//
// Created by dan on 24-3-11.
//

#ifndef IPCBOZO_BOZO_H
#define IPCBOZO_BOZO_H

#include <string>
#include <vector>
#include <iostream>
#include <type_traits>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gprintf.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "json.hpp"

class IpcBozo {
 public:

  // client and server should set same ipc_channel to communicate
  IpcBozo(const std::string &ipc_channel) {
    dbus_server_name = "com.ipc_bozo_" + ipc_channel + ".server";
    dbus_object_name = "/com/ipc_bozo_" + ipc_channel + "/object";
    dbus_interface_name = "com.ipc_bozo_" + ipc_channel + ".interface";
  }

  bool client_init() {
    GError *err = nullptr;

    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
    if (err) {
      g_printf("client_init():g_bus_get_sync() failed: %s\n", err->message);
      g_clear_error(&err);
      return false;
    }

    client_dbus_proxy = g_dbus_proxy_new_sync(conn,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              nullptr,
                                              dbus_server_name.c_str(),
                                              dbus_object_name.c_str(),
                                              dbus_interface_name.c_str(),
                                              nullptr,
                                              &err);
    if (err) {
      g_printf("client_init():g_dbus_proxy_new_sync() failed: %s\n", err->message);
      g_clear_error(&err);
      return false;
    }

    g_dbus_connection_signal_subscribe(conn,
                                      dbus_server_name.c_str(),
                                      dbus_interface_name.c_str(),
                                      SignalName,
                                      dbus_object_name.c_str(),
                                      nullptr,
                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                      on_handle_signal,
                                      this,
                                      nullptr);
    return true;
  }

  bool server_init() {
    DBusError err;
    dbus_error_init(&err);

    server_dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!server_dbus_connection) {
      g_printf("server_init(): dbus_bus_get() failed: %s\n", err.message);
      goto fail;
    }

    if (dbus_bus_request_name(server_dbus_connection, dbus_server_name.c_str(), 
                              DBUS_NAME_FLAG_REPLACE_EXISTING, &err) !=
        DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
      g_printf("server_init(): dbus_bus_request_name() failed: %s\n", err.message);
      goto fail;
    }

    if (!dbus_connection_register_object_path(server_dbus_connection, 
                                              dbus_object_name.c_str(), 
                                              &dbus_server_v_table, this)) {
      g_printf("server_init(): dbus_connection_register_object_path() failed!\n");
      goto fail;
    }

    dbus_error_free(&err);
    return true;

    fail:
    dbus_error_free(&err);
    return false;
  }

  void server_loop_run() {
    auto mainloop = g_main_loop_new(nullptr, false);
    dbus_connection_setup_with_g_main(server_dbus_connection, nullptr);
    g_main_loop_run(mainloop);
  }

  static void client_loop_run() {
    g_main_loop_run(g_main_loop_new(nullptr, false));
  }

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

  template<typename T, typename... Ts>
  bool remote_call(T *out_arg, int method_id, Ts... in_args) {
    std::string call_ret;
    auto call_succeed = dbus_call(serialize(method_id, in_args...), &call_ret);
    *out_arg = stringTo<T>(call_ret);

    return call_succeed;
  }

  bool remote_call(int method_id) {
    return dbus_call(serialize(method_id), nullptr);
  }

  template<typename... Ts>
  bool remote_call(int method_id, Ts... in_args) {
    return dbus_call(serialize(method_id, in_args...), nullptr);
  }

  template<typename T>
  bool remote_call(T *out_arg, int method_id) {
    std::string call_ret;
    auto call_succeed = dbus_call(serialize(method_id), &call_ret);
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
    .unregister_function = nullptr,
    .message_function = server_message_handler
  };

  static void on_handle_signal(GDBusConnection *,
                               const gchar *,
                               const gchar *,
                               const gchar *,
                               const gchar *,
                               GVariant *parameters,
                               gpointer object) {
    auto bozo = (IpcBozo *) object;
    const gchar *arg;
    g_variant_get(parameters, "(&s)", &arg);

    auto signal_id = IpcBozo::get_method_signal_id(arg);
    auto arg_vec = IpcBozo::get_arg_vec(arg);

    if (!bozo->signal_handler_registered(signal_id)) {
      return;
    }

    if (bozo->signal_handler_map.find(signal_id) != bozo->signal_handler_map.end()) {
      bozo->signal_handler_map[signal_id](arg_vec);
    } else if (bozo->signal_handler_map_no_arg.find(signal_id) != bozo->signal_handler_map_no_arg.end()) {
      bozo->signal_handler_map_no_arg[signal_id]();
    }
  }

  static DBusHandlerResult server_message_handler(DBusConnection *conn, 
                                                  DBusMessage *message, 
                                                  void *object) {
    auto bozo = (IpcBozo *) object;

    DBusHandlerResult dbus_handler_ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    DBusMessage *dbus_ret = nullptr;
    DBusError err;
    std::string method_ret = "void";
    const char *method_ret_cstr;
    char *method_arg_cstr = nullptr;

    dbus_error_init(&err);

    if (!dbus_message_is_method_call(message, bozo->dbus_interface_name.c_str(), MethodName)) {
      goto end;
    }

    dbus_message_get_args(message, &err, DBUS_TYPE_STRING, &method_arg_cstr, DBUS_TYPE_INVALID);
    if (dbus_error_is_set(&err)) {
      g_printf("server_message_handler():dbus_message_get_args() failed\n");
      goto end;
    }

    if (!bozo->call_method(method_arg_cstr, &method_ret)) {
      g_printf("server_message_handler():call_method() failed!\n");
      goto end;
    }

    dbus_ret = dbus_message_new_method_return(message);
    if (!dbus_ret) {
      g_printf("server_message_handler():dbus_message_new_method_return() failed\n");
      goto end;
    }

    method_ret_cstr = method_ret.c_str();
    if (!dbus_message_append_args(dbus_ret, DBUS_TYPE_STRING, &method_ret_cstr, DBUS_TYPE_INVALID)) {
      g_printf("server_message_handler():dbus_message_append_args() failed\n");
      goto end;
    }

    if (!dbus_connection_send(conn, dbus_ret, nullptr)) {
      g_printf("server_message_handler():dbus_connection_send() failed\n");
      goto end;
    }

    dbus_handler_ret = DBUS_HANDLER_RESULT_HANDLED;

    end:
    if (dbus_ret) { dbus_message_unref(dbus_ret); }

    dbus_error_free(&err);
    return dbus_handler_ret;
  }
  static int get_method_signal_id(const std::string &arg) {
    nlohmann::json j = nlohmann::json::parse(arg);

    assert(j.contains(MethodSignalIdKey));

    return std::stoi(to_string(j[MethodSignalIdKey]));
  }

  static std::vector<std::string> get_arg_vec(const std::string &arg) {
    nlohmann::json j = nlohmann::json::parse(arg);

    if (j.contains(ArgVecKey)) {
      return j[ArgVecKey];
    }

    return {};
  }
  bool dbus_call(const std::string &in_arg, std::string *out_arg) {
    GError *err = nullptr;
    GVariant *ret_gvar = g_dbus_proxy_call_sync(client_dbus_proxy,
                                                MethodName,
                                                g_variant_new("(s)", in_arg.data()),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                nullptr,
                                                &err);
    if (err) {
      g_printf("dbus_call(): g_dbus_proxy_call_sync() failed: %s\n", err->message);
      g_clear_error(&err);
      return false;
    }

    if (out_arg) {
      gchar *ret_str = nullptr;
      g_variant_get(ret_gvar, "(&s)", &ret_str);
      *out_arg = ret_str;
    }

    g_variant_unref(ret_gvar);
    return true;
  }

  void dbus_send_signal(const std::string &arg) {

    DBusMessage *message = dbus_message_new_signal(dbus_object_name.c_str(), 
                                                   dbus_interface_name.c_str(), 
                                                   SignalName);
    if (!message) {
      g_printf("dbus_send_signal():dbus_message_new_signal() failed");
      return;
    }

    const char *arg_cstr = arg.c_str();
    if (!dbus_message_append_args(message, DBUS_TYPE_STRING, &arg_cstr, DBUS_TYPE_INVALID)) {
      g_printf("dbus_send_signal():dbus_message_append_args() failed");
      return;
    }

    if (!dbus_connection_send(server_dbus_connection, message, nullptr)) {
      g_printf("dbus_send_signal():dbus_connection_send() failed");
      return;
    }
  }

  bool signal_handler_registered(int signal_id) {
    if (signal_handler_map.find(signal_id) != signal_handler_map.end()) {
      return true;
    }
    if (signal_handler_map_no_arg.find(signal_id) != signal_handler_map_no_arg.end()) {
      return true;
    }

    return false;
  }

  bool call_method(const std::string &arg, std::string *ret) {
    auto method_id = get_method_signal_id(arg);
    auto method_arg_vec = get_arg_vec(arg);

    if (method_map.find(method_id) != method_map.end()) {
      *ret = method_map[method_id](method_arg_vec);
    } else if (method_map_no_ret.find(method_id) != method_map_no_ret.end()) {
      method_map_no_ret[method_id](method_arg_vec);
    } else if (method_map_no_arg.find(method_id) != method_map_no_arg.end()) {
      *ret = method_map_no_arg[method_id]();
    } else if (method_map_no_ret_no_arg.find(method_id) != method_map_no_ret_no_arg.end()) {
      method_map_no_ret_no_arg[method_id]();
    } else {
      g_printf("call_method():no such method(id:%d) registered by server!\n", method_id);
      return false;
    }

    return true;
  }

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
    if constexpr (std::is_convertible_v<T, std::string>) {
      arg_vec->push_back(std::string(arg));
    } else {
      arg_vec->push_back(toString(arg));
    }
  }

  template<typename T, typename... Ts>
  void toVector(std::vector<std::string> *arg_vec, T arg1, Ts... args) {
    toVector(arg_vec, arg1);
    if constexpr (sizeof...(args) > 0) {
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
