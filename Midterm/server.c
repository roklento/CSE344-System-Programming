#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

#define MAX_CLIENTS 5

struct request {
    pid_t pid;
    char command[100];
};

char server_fifo[100];
int server_fd;
int dummy_fd;
int client_pids[MAX_CLIENTS];
int num_clients = 0;
struct request req;


void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\n>> kill signal from client.. terminating...\n");
        close(server_fd);
        unlink(server_fifo);
        printf(">> bye\n");
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: server <dirname> <max. #ofClients>\n");
        exit(1);
    }

    char* dirname = argv[1];
    int max_clients = atoi(argv[2]);

    printf(">> Server Started PID %d…\n", getpid());
    printf(">> waiting for clients...\n");

    sprintf(server_fifo, "/tmp/server.%d.fifo", getpid());
    if (mkfifo(server_fifo, 0666) < 0) {
        perror("mkfifo");
        exit(1);
    }

    server_fd = open(server_fifo, O_RDONLY);
    if (server_fd < 0) {
        perror("open");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    while (1) {
        pid_t finished_pid;
        int status;
        while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            // A child process has finished
            for (int i = 0; i < num_clients; ++i) {
                if (client_pids[i] == finished_pid) {
                    printf(">> Client PID %d disconnected\n", finished_pid);
                    // Remove the disconnected client PID from the list.
                    for (int j = i; j < num_clients - 1; ++j) {
                        client_pids[j] = client_pids[j + 1];
                    }
                    --num_clients;
                    break;  // Break the loop to avoid creating a new client connection.
                }
            }
        }

        ssize_t num_bytes = read(server_fd, &req, sizeof(struct request));
        if (num_bytes <= 0) {
            perror("read");
            continue;
        }

        dummy_fd = open(server_fifo, O_WRONLY);
        if (dummy_fd < 0) {
            perror("open");
            exit(1);
        }

        if (num_clients >= MAX_CLIENTS) {
            printf(">> Max clients reached. Ignoring client PID %d\n", req.pid);
            continue;
        }

        int client_pid = req.pid;
        printf(">> Client PID %d connected as “client%d”\n", client_pid, num_clients+1);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } 
        else if (pid == 0) {
            // Child process
            char client_fifo[100];
            sprintf(client_fifo, "/tmp/client.%d.fifo", client_pid);
            int client_fd = open(client_fifo, O_RDWR);
            if (client_fd < 0) {
                printf(">> Client PID %d not found\n", client_pid);
                exit(1);
            }

            while (1) {
                // Read the command from the client
                ssize_t num_bytes = read(client_fd, &req, sizeof(struct request));
                if (num_bytes <= 0) {
                    printf(">> Client PID %d disconnected\n", client_pid);
                    _exit(0);  // Notify the parent process that this child process has exited.
                }

                if (strcmp(req.command, "quit") == 0) {
                    close(client_fd);
                    exit(0);  // Notify the parent process that this child process has exited.
                } 
                else if (strcmp(req.command, "killServer") == 0) {
                    raise(SIGINT);
                }
                else if (strcmp(req.command, "list") == 0) {
                    char file_list[1024] = "";  // Make sure this is large enough
                    DIR *d;
                    struct dirent *dir;
                    d = opendir(dirname);
                    if (d) {
                        while ((dir = readdir(d)) != NULL) {
                            strcat(file_list, dir->d_name);
                            strcat(file_list, "\n");
                        }
                        closedir(d);
                    }
                    write(client_fd, file_list, strlen(file_list) + 1);
                }
            }
        }
        else {
            // Parent process
            client_pids[num_clients] = pid;
            ++num_clients;
        }
        
    }
    return 0;
}

           
