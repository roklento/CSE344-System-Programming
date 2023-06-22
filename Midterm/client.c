
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

struct request {
    pid_t pid;
    char command[100];
};

struct response {
    int seqNum;
};

char server_fifo[100];
char client_fifo[100];
struct request req;
struct response resp;



int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: client <Connect/tryConnect> <serverPID>\n");
        exit(1);
    }
    char* connection_type = argv[1];
    int server_pid = atoi(argv[2]);
    
    sprintf(server_fifo, "/tmp/server.%d.fifo", server_pid);
    sprintf(client_fifo, "/tmp/client.%d.fifo", getpid());

    mkfifo(client_fifo, 0666);

    int server_fd = open(server_fifo, O_WRONLY);
    if (server_fd == -1) {
        perror("open");
        exit(1);
    }

    req.pid = getpid();
    write(server_fd, &req, sizeof(struct request));

    int client_fd = open(client_fifo, O_RDONLY);
    if (client_fd == -1) {
        perror("open");
        exit(1);
    }

    char command[100];
    while (1) {
        printf(">> Enter command: ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;
        strcpy(req.command, command);

        if (strcmp(command, "help") == 0) {
            printf("Possible commands:\n");
            printf("quit - disconnect from the server\n");
            printf("killServer - send a kill signal to the server\n");
            printf("list - list files in server's directory\n");
        } else if (strcmp(command, "quit") == 0) {
            write(server_fd, &req, sizeof(struct request));
            close(client_fd);
            close(server_fd);
            unlink(client_fifo);
            break;
        } else if (strcmp(command, "killServer") == 0) {
             write(server_fd, &req, sizeof(struct request));  
            close(client_fd);
            close(server_fd);
            unlink(client_fifo);
            break;
        } else if (strcmp(command, "list") == 0) {
            write(server_fd, &req, sizeof(struct request)); 
            char file_list[1024];  // Make sure this is large enough
            read(client_fd, file_list, sizeof(file_list));
            printf("Files in server's directory:\n%s\n", file_list);
        } else {
            write(server_fd, command, strlen(command) + 1);
        }
    }
    close(client_fd);
    close(server_fd);
    unlink(client_fifo);

    return 0;
}
