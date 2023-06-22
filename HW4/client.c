#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>

struct request {
    pid_t pid;
    char command[100];
};

struct request req;
char server_fifo[100];
char client_fifo[100];
int server_fd;
int client_fd;

void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\n>> terminating client...\n");
        close(server_fd);
        close(client_fd);
        unlink(client_fifo);
        printf(">> bye\n");
        exit(0);
    }
}

void* connect_server(void* args){
    server_fd = open(server_fifo, O_WRONLY);
    if (server_fd < 0) {
        perror("open");
        exit(1);
    }

    strcpy(req.command, client_fifo);
    write(server_fd, &req, sizeof(struct request));

    client_fd = open(client_fifo, O_RDONLY);
    if (client_fd < 0) {
        perror("open");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: client Connect <server_pid>\n");
        exit(1);
    }

    if (strcmp(argv[1], "Connect") != 0) {
        fprintf(stderr, "The first argument should be 'Connect'\n");
        exit(1);
    }

    req.pid = getpid();
    sprintf(server_fifo, "/tmp/server.%s.fifo", argv[2]);
    sprintf(client_fifo, "/tmp/client.%d.fifo", getpid());

    if (mkfifo(client_fifo, 0666) < 0) {
        perror("mkfifo");
        exit(1);
    }

    pthread_t connect_thread;
    pthread_create(&connect_thread, NULL, connect_server, NULL);

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while (1) {
        printf("enter command: ");
        fgets(req.command, sizeof(req.command), stdin);
        req.command[strcspn(req.command, "\n")] = '\0'; 


        if (strcmp(req.command, "help") == 0) {
            printf("Possible commands:\n");
            printf("quit - disconnect from the server\n");
            printf("killServer - send a kill signal to the server\n");
            printf("list - list files in server's directory\n");
        }

        else if (strcmp(req.command, "quit") == 0) {
            printf(">> Client PID %d is quitting...\n", req.pid);
            close(server_fd);
            close(client_fd);
            unlink(client_fifo);
            break;
        }
        else if (strcmp(req.command, "killServer") == 0) {
            write(server_fd, &req, sizeof(struct request));  
            close(client_fd);
            close(server_fd);
            unlink(client_fifo);
            break;
        }
        else if (strcmp(req.command, "list") == 0) {
            write(server_fd, &req, sizeof(struct request)); 
            char file_list[1024]; 
            read(client_fd, file_list, sizeof(file_list));
            printf("Files in server's directory:\n%s\n", file_list);
        } 
        else {
            write(server_fd, req.command, strlen(req.command) + 1);
        }
        char buf[100];
        while (read(client_fd, buf, sizeof(buf)) > 0) {
            printf("%s", buf); 
        }
    }

    return 0;
}