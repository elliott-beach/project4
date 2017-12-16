#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main() {
  int fildes = open("/dev/scullBuffer0", O_RDONLY);
  if(fildes < 0)
    return 1;
  //  sleep(5);
  char data[512] = "\n";

  read(fildes, data, 512);
  
  printf("%s\n", data);
  close(fildes);
  return 0;
}
