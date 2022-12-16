#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#define BUF_SIZE 512

int
main(int argc, char** argv)
{
  char *new_argv[MAXARG];
  char buf[BUF_SIZE];
  int new_argc = 0;
  if (argc == 1) {
    new_argv[0] = malloc(sizeof("echo"));
    memcpy(new_argv[0], "echo", sizeof("echo"));
    new_argc++;
  } else {
    for (; new_argc < argc-1; new_argc++) {
      int i = new_argc;
      new_argv[i] = argv[i+1];
    }
  }

  int buf_end = 0, buf_tmp = 0;
  while (read(0, buf+buf_end, 1) == 1) {
    buf_end++;
    if (buf_end >= BUF_SIZE) {
        fprintf(2, "error: args too long\n");
        exit(1);
    }
  }
    
  buf[buf_end-1] = 0;

  while (buf_tmp < buf_end) {
    while (buf_tmp < buf_end) {
      if (buf[buf_tmp] <= 32)
        buf[buf_tmp++] = 0;
      else 
        break;
    }
    new_argv[new_argc] = buf + buf_tmp;
    while (buf_tmp < buf_end && buf[buf_tmp] > 32) {
      buf_tmp++;
    }
    
    new_argc++;
  }
  new_argv[new_argc] = 0;

  if (fork() == 0) {
    exec(new_argv[0], new_argv);
    fprintf(2, "exec %s error\n", new_argv[0]);
    exit(1);
  } else {
    wait(0);
  }

  exit(0);
}