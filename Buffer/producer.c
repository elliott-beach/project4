#include <unistd.h>
#include <fcntl.h>

int main() {
    int num = argv[0];
    char msg[18];
    sprintf(msg, "I am producer #\n");
    int fildes = open("/dev/scullBuffer0", O_WRONLY);
    if(fildes < 0)
	return 1;
    
    if(write(fildes, msg, 18) != 18) {
	return -1;
    }

    close(fildes);

    return 0;
}
