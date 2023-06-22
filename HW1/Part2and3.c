#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

int MyDup(int oldfd);
int MyDup2(int oldfd, int newfd);

int main()
{
    off_t offset1, offset2;

    //This is opening the file test.txt and if it is not able to open it, it will print out an error message.
    int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1) {
        perror("cant open file");
        return 1;
    }

    int newfd = MyDup(fd);
    if (newfd == -1) {
        perror("cant dup file");
        return 1;
    }

    int newfd2 = MyDup2(fd, 10);
    if (newfd2 == -1) {
        perror("cant dup2 file");
        return 1;
    }

    write(fd, "write: fd\n", 10);
    write(newfd, "write: newfd\n", 13);
    write(newfd2, "write: newfd2\n", 14);

    //Setting the offset of thclee file to 20.
    offset1 = lseek(newfd, 20, SEEK_SET);
    if (offset1 < 0) {
        close(newfd);
        close(newfd2);
        return 1;
    }

    // Read the file offset of the second file descriptor
    offset2 = lseek(newfd2, 0, SEEK_CUR);
    if (offset2 < 0) {
        close(newfd);
        close(newfd2);
        return 1;
    }

    // Compare the two offsets
    if (offset1 == offset2) {
        printf("File descriptors that have been duplicated will have a shared file offset.\n");
    } else {
        printf("If file descriptors are duplicated, they will have separate file offsets and will not share the same file offset.\n");
    }
    
    // print offset1 value
    //printf("offset1: %ld \t offset2: %ld \t fd: %d \t newfd: %d \t newfd2: %d \t \t" , offset1, offset2, fd, newfd, newfd2);

    close(fd);
    close(newfd);
    close(newfd2);

}

/**
 * Duplicate the file descriptor oldfd, using the lowest-numbered available file descriptor greater
 * than or equal to arg.
 * 
 * @param oldfd The file descriptor to be duplicated.
 * 
 * @return The new file descriptor.
 */
int MyDup(int oldfd) {

    //Duplicating the file descriptor.
    return fcntl(oldfd, F_DUPFD);
}

/**
 * This function checks if the oldfd is already open. If it is, it closes it. Then it checks if the
 * newfd is already open. If it is, it closes it. Then it checks if the oldfd is already open. If it
 * is, it closes it
 * 
 * @param oldfd The file descriptor to be duplicated.
 * @param newfd The new file descriptor.
 * 
 * @return The newfd is being returned.
 */
int MyDup2(int oldfd, int newfd) {
    
    //This is checking if the oldfd is already open. If it is, it closes it.
    if (oldfd == newfd) {
        if (fcntl(oldfd, F_GETFL) == -1) {
            errno = EBADF;
            return -1;
        }
        return oldfd;
    }

    //Checking if the newfd is already open. If it is, it closes it.
    if (fcntl(newfd, F_GETFL) != -1) {
        close(newfd);
    }

    //Checking if the oldfd is already open. If it is, it closes it.
    if (fcntl(oldfd, F_DUPFD, newfd) == -1) {
        return -1;
    }

    return newfd;
}

