//
// Created by dan on 24-3-12.
//

#include "bozo.h"

int IpcBozo::get_method_signal_id(const std::string &arg) {
  nlohmann::json j = nlohmann::json::parse(arg);

  assert(j.contains(MethodSignalIdKey));

  return std::stoi(to_string(j[MethodSignalIdKey]));
}

std::vector<std::string> IpcBozo::get_arg_vec(const std::string &arg) {
  nlohmann::json j = nlohmann::json::parse(arg);

  if (j.contains(ArgVecKey)) {
    return j[ArgVecKey];
  }

  return {};
}

bool IpcBozo::signal_handler_registered(int signal_id) {
  if (signal_handler_map.find(signal_id) != signal_handler_map.end()) {
    return true;
  }
  if (signal_handler_map_no_arg.find(signal_id) != signal_handler_map_no_arg.end()) {
    return true;
  }

  return false;
}

void IpcBozo::on_handle_signal(GDBusConnection *,
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

bool IpcBozo::call_method(const std::string &arg, std::string *ret) {
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

DBusHandlerResult IpcBozo::server_message_handler(DBusConnection *conn, DBusMessage *message, void *object) {
  auto bozo = (IpcBozo *) object;

  DBusHandlerResult dbus_handler_ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  DBusMessage *dbus_ret = nullptr;
  DBusError err;
  std::string method_ret = "void";
  const char *method_ret_cstr;
  char *method_arg_cstr = nullptr;

  dbus_error_init(&err);

  if (!dbus_message_is_method_call(message, bozo->dbus_interface_name.c_str(), MethodName)) {
    g_printf("server_message_handler():dbus_message_is_method_call() failed\n");
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

bool IpcBozo::client_init() {
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

bool IpcBozo::server_init() {
  DBusError err;
  dbus_error_init(&err);

  server_dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (!server_dbus_connection) {
    g_printf("server_init(): dbus_bus_get() failed: %s\n", err.message);
    goto fail;
  }

  if (dbus_bus_request_name(server_dbus_connection, dbus_server_name.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &err) !=
      DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    g_printf("server_init(): dbus_bus_request_name() failed: %s\n", err.message);
    goto fail;
  }

  if (!dbus_connection_register_object_path(server_dbus_connection, dbus_object_name.c_str(), &dbus_server_v_table, this)) {
    g_printf("server_init(): dbus_connection_register_object_path() failed!\n");
    goto fail;
  }

  dbus_error_free(&err);
  return true;

  fail:
  dbus_error_free(&err);
  return false;
}

void IpcBozo::server_loop_run() {
  auto mainloop = g_main_loop_new(nullptr, false);
  dbus_connection_setup_with_g_main(server_dbus_connection, nullptr);
  g_main_loop_run(mainloop);
}

void IpcBozo::client_loop_run() {
  g_main_loop_run(g_main_loop_new(nullptr, false));
}

void IpcBozo::dbus_send_signal(const std::string &arg) {

  DBusMessage *message = dbus_message_new_signal(dbus_object_name.c_str(), dbus_interface_name.c_str(), SignalName);
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

bool IpcBozo::dbus_call(const std::string &in_arg, std::string *out_arg) {
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
