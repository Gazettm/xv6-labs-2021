#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int split(char *buf, char *words[])
{
    int count = 0;
    int in_word = 0;
    for (char *p = buf; *p; p++) {
        if (*p == ' ' || *p == '\t') {
            in_word = 0;
            *p = 0;
        } else if (!in_word) {
            words[count++] = p;
            in_word = 1;
        }
    }
    return count;
}

int main(int argc, char *argv[])
{
    char buf[512];
    int n = 0;
    char c;

    while (read(0, &c, 1) == 1) {
        if (c == '\n') {
            buf[n] = 0;

            if (n > 0) {

                char *new_argv[MAXARG];
                for (int i = 0; i < argc; i++) new_argv[i] = argv[i];
                int words = split(buf, &new_argv[argc]);
                new_argv[argc + words] = 0;

                if (fork() == 0) {
                    exec(new_argv[1], new_argv + 1);
                    printf("exec failed\n");
                    exit(1);
                }
                wait(0);
            }
            n = 0;
        } else {
            buf[n++] = c;
        }
    }
    exit(0);
}