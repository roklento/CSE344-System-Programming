#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

int main(int argc, char *argv[]) {
    /* This is checking if the user has given enough arguments to the program. If not, it prints out
    the correct input format and returns 1. */
    if (argc < 3) {
        printf("Too few arguments given to program %s \n", argv[0]);
        printf("Correct Input Format: %s filename num-bytes [x]\n", argv[0]);
        return 1;
    }

    /* This is declaring the variables that will be used in the program. The first two are the filename
    and the number of bytes that will be written to the file. The third is the byte that will be
    written to the file. The last is a boolean that will be used to determine if the program will
    use lseek or not. */
    char *filename = argv[1];
    int num_bytes = atoi(argv[2]);
    char byte = 'A';
    bool isLseek = false;

    /* This is checking if the user has given the program the optional argument. If they have, it sets
    the boolean isLseek to true. */
    if (argc == 4) {
        if (argv[3][0] == 'x') {
            isLseek = true;
        }
    }
    int fd;
    /* This is checking if the user has given the program the optional argument. If they have, it sets
        the boolean isLseek to true. */
    if(isLseek) {
        fd = open(filename, O_RDWR | O_CREAT);
    } else {
        fd = open(filename, O_RDWR | O_CREAT | O_APPEND);
    }

    if (fd == -1) {
        perror("cant open file");
        return 1;
    }

    /* This is a for loop that is writing the byte to the file. The for loop is going to run for the
    number of bytes that the user has specified. If the user has specified the optional argument,
    then the program will use lseek to move the file pointer to the end of the file. Then, the
    program will write the byte to the file. */
    for (int i = 0; i < num_bytes; i++) {
        if (isLseek) {
            lseek(fd, 0, SEEK_END);
        }
        write(fd, &byte, 1);
    }

    close(fd);
    return 0;
}