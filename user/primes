#include "kernel/types.h"
#include "user/user.h"

void sieve(int left_fd)
{
    int n;
    if(read(left_fd, &n, 4) <= 0){
        exit(0);
    }

    printf("prime %d\n", n);
    int p[2];
    pipe(p);
    if(fork() == 0){
        close(p[1]);
        close(left_fd);
        sieve(p[0]);
        exit(0);
    }
    close(p[0]);

    int num;
    while(read(left_fd, &num, 4) == 4){
        if(num % n != 0){
            write(p[1], &num, 4);
        }
    }
    close(p[1]);
    wait(0);
    exit(0);
}

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    
    if(fork() == 0){
        close(p[1]);
        sieve(p[0]);
        exit(0);
    }
    close(p[0]);
    for(int i = 2;i <= 35; i++){
        write(p[1], &i, 4);
    }
    close(p[1]);
    wait(0);
    exit(0);
}