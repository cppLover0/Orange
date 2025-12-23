#include <stdio.h>
#include <locale.h>
#include <langinfo.h>

int main() {
    // Устанавливаем локаль по умолчанию
    setlocale(LC_ALL, "");

    // Пытаемся получить информацию о правилах сортировки (COLLATION)
    // В POSIX это не предусмотрено напрямую, так что используем другие значения
    printf("Hi: %s\n", nl_langinfo(CODESET));

    return 0;
}