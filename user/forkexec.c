#include "kernel/types.h"
#include "user/user.h"
// forkexec.c: fork then exec
int
main()
{
  int pid, status;
  pid = fork();
  if (pid == 0) {
    char* argv[] = { "echo", "THIS", "IS", "ECHO", 0 };
    exec("echo0", argv);
    printf("exec faild!\n");
    exit(1);
  }
  else {
    printf("parent waitting\n");
    wait(&status);
    printf("thi child exited with status %d\n", status);

    exit(0);
  }
}
