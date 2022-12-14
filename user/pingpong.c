#include "kernel/types.h"
#include "user/user.h"

int
main()
{
  int p[2];
  pipe(p);
  if (fork() == 0) {
    char buf[2];
    read(p[0], buf, 1);
    //printf("%d: received ping, %c\n", getpid(), buf[0]);
    printf("%d: received ping\n", getpid());
    write(p[1], "q", 1);
  } else {
    char buf[2];
    write(p[1], "p", 1);
    read(p[0], buf, 1);
    //printf("%d: received pong, %c\n", getpid(), buf[0]);
    printf("%d: received pong\n", getpid());
  }
  exit(0);
}
