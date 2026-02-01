#define _GNU_SOURCE
#include <dirent.h>     /* Содержит структуру linux_dirent64 */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define BUF_SIZE 1024

struct linux_dirent64 {
    unsigned long long d_ino;    /* Inode number */
    long long          d_off;    /* Offset to next dirent */
    unsigned short     d_reclen; /* Length of this dirent */
    unsigned char      d_type;   /* File type */
    char               d_name[]; /* Filename (null-terminated) */
};

int main() {
    int fd = open(".", O_RDONLY | O_DIRECTORY);
    char buf[BUF_SIZE];
    int nread;

    while ((nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE)) > 0) {
        for (int bpos = 0; bpos < nread; ) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
            
            // Выводим тип и имя (DT_DIR = каталог, DT_REG = обычный файл)
            printf("%-10s  %s\n", 
                (d->d_type == DT_DIR) ? "directory" : "file", 
                d->d_name);

            bpos += d->d_reclen; // Смещение к следующей записи
        }
    }
    close(fd);
    return 0;
}