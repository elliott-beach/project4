#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  int produce = 1;
  char *argv_consume[] = {"./consumer", NULL};
  char *argv_produce[] = {"./producer", NULL};
  int pid;

  for(int i = 0; i < 100; i++) {
    pid = fork();
    if(pid == 0) {
      if(produce) {
	execv("./producer", argv_produce);
      }
      else {
	execv("./consumer", argv_consume);
      }
    }
    produce = 1 - produce;
  }
}
