#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

void 
find(char *path, char *filename) 
{
  int fd;
  struct dirent de;
  struct stat st;
  char buf[512];

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0) {
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
  
  strcpy(buf, path);
  buf[strlen(path)] = '/';
  char *p = buf + strlen(path) + 1;

  switch(st.type) {
  case T_DEVICE:
  case T_FILE:
    fprintf(2, "%s is not a directory\n", path);
    break;
  case T_DIR:
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      if (!strcmp(".", de.name) || !strcmp("..", de.name))
        continue;
      
      memmove(p, &de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        fprintf(2, "ls: cannot stat %s\n", buf);
        continue;
      }
      switch (st.type)
      {
      case T_DIR:
        find(buf, filename);
        break;
        
      default:
        if(!strcmp(filename, de.name)) {
          printf("%s\n", buf);
        }
      }
    }  
  }
}

int
main(int argc, char **argv)
{
  if (argc != 3) {
    printf("Usage: find <path> <filename>\n");
  }
  find(argv[1], argv[2]);
  exit(0);
}