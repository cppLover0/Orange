#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    DBusConnection *conn;
    DBusError err;
    DBusMessage *msg;
    DBusMessage *reply;
    DBusError err_reply;

    // Инициализация ошибок
    dbus_error_init(&err);
    dbus_error_init(&err_reply);

    // Получение соединения с сеансовым bus
    int b = 0;
    printf("breakpoint %d\n",b++);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    printf("breakpoint %d\n",b++);
    
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
        return 1;
    }
    printf("breakpoint %d\n",b++);
    if (NULL == conn) {
        fprintf(stderr, "Failed to connect to the D-Bus session bus\n");
        return 1;
    }
    printf("breakpoint %d\n",b++);
    // Создаём сообщение для вызова метода
    msg = dbus_message_new_method_call(
        "com.example.Hello",              // название сервиса
        "/com/example/HelloObject",        // объект
        "com.example.Hello",               // интерфейс
        "SayHello"                         // метод
    );
    printf("breakpoint %d\n",b++);

    if (NULL == msg) {
        fprintf(stderr, "Message Null\n");
        return 1;
    }
    printf("breakpoint %d\n",b++);
    // Отправка сообщения и ожидание ответа
    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    printf("breakpoint %d\n",b++);
    dbus_message_unref(msg);
    printf("breakpoint %d\n",b++);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Error in method call: %s\n", err.message);
        dbus_error_free(&err);
        return 1;
    }   
    printf("breakpoint %d\n",b++);
    // Получение строки из ответа
    const char *reply_str;
    if (dbus_message_get_args(reply, &err,
                              DBUS_TYPE_STRING, &reply_str,
                              DBUS_TYPE_INVALID)) {
        printf("Response: %s\n", reply_str);
    } else {
        fprintf(stderr, "Failed to get args: %s\n", err.message);
        dbus_error_free(&err);
        return 1;
    }
    printf("breakpoint %d\n",b++);
    dbus_message_unref(reply);
    return 0;
}