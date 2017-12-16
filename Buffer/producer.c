#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    char msg[1024];
    char* num = argv[1];
    sprintf(msg, "I am producer #%s\n", num);
    int fildes = open("/dev/scullBuffer0", O_WRONLY);
    if(fildes < 0)
	return 1;
    
    if(write(fildes, msg, strlen(msg)) != strlen(msg)) {
        printf("producer %s: writing failed\n", num);
	return -1;
    }

    close(fildes);

    return 0;
}
