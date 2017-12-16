#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  char num [1024];
  char *argv_consume[] = {"./consumer", num, NULL};
  char *argv_produce[] = {"./producer", num, NULL};

  for(int i = 0; i < 100; i++) {
    int pid = fork();
    if(pid == 0) {
      sprintf(num, "%d", i/2);
      if(i & 1) {
	execv("./producer", argv_produce);
      } else {
	execv("./consumer", argv_consume);
      }
    }
  }
}
