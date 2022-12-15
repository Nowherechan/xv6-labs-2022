#include "kernel/types.h"
#include "user/user.h"

void 
pipe_primes(int pipe_read)
{
  int x, min_x;
  
  if (read(pipe_read, &min_x, 4) == 4) {
    
    printf("prime %d\n", min_x);

    int child_p[2];
    pipe(child_p);

    if (fork() == 0) {
      close(pipe_read);
      close(child_p[1]);
      pipe_primes(child_p[0]);
    } else {
      close(child_p[0]);
      while (read(pipe_read, &x, 4) == 4) {
        if (x % min_x) {
          write(child_p[1], &x, 4);
        }
      }
    }
    close(child_p[1]);
  }
  close(pipe_read);
  return;
}

int
main()
{
  int p[2];
  pipe(p);

  for (int i = 2; i <= 35; i++) {
    write(p[1], &i, 4);
  }
  close(p[1]);
  pipe_primes(p[0]);
  wait(0);
  exit(0);
}
