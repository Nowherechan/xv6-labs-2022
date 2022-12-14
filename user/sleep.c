#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Usage: sleep <ticks>, where ticks is a integer\n");
    exit(-1);
  } else {
    int ticks = atoi(argv[1]);
    sleep(ticks);
  }
  exit(0);
}