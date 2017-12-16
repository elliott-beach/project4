#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char** argv) {
  int fildes = open("/dev/scullBuffer0", O_RDONLY);
  if(fildes < 0) {
    printf("opening failed\n");
    return 1;
  }

  char data[512];
  int count = read(fildes, data, 512);
  if (count <= 0){
    printf("reading failed\n");
  } else {
    printf("read message: %s\n", data);
  }
  close(fildes);
  return 0;
}
