#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>


struct request {
    pid_t pid;
    char command[100];
};

char server_fifo[100];
int server_fd;
int dummy_fd;
int client_pids[100];
int num_clients = 0;
struct request req;

char* dirname;

void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\n>> kill signal from client.. terminating...\n");
        close(server_fd);
        unlink(server_fifo);
        printf(">> bye\n");
        exit(0);
    }
}

int client_already_connected(pid_t pid) {
    for (int i = 0; i < num_clients; i++) {
        if (client_pids[i] == pid) {
            return 1;
        }
    }
    return 0;
}

void write_server_log(pid_t client_pid, const char* message) {
    printf("%s\n", message);
    char log_file[200];
    sprintf(log_file, "%s/client_%d.log", dirname, client_pid);

    FILE* log = fopen(log_file, "a");
    if (log == NULL) {
        perror("fopen");
        return;
    }

    fprintf(log, "%s\n", message);
    fclose(log);
}

void *process_client_request(void *client_fd_ptr) {
    int client_fd = *(int *)client_fd_ptr;
    free(client_fd_ptr);

    struct request req;
    pid_t client_pid = -1;

    ssize_t num_bytes;
    while ((num_bytes = read(client_fd, &req, sizeof(struct request))) > 0) {
        client_pid = req.pid;

        char log_message[200];
        sprintf(log_message, "Client %d command: %s", client_pid, req.command);
        write_server_log(client_pid, log_message);  

        if (strcmp(req.command, "quit") == 0) {
            printf(">> Client PID %d has quit\n", client_pid);
            break;  
        }

        else if (strcmp(req.command, "killServer") == 0) {
                    raise(SIGINT);
                }

        else if (strcmp(req.command, "list") == 0) {
            DIR *dir = opendir(dirname);
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                write(client_fd, entry->d_name, strlen(entry->d_name) + 1);
            }
            closedir(dir);
        }
    }
    if(num_bytes <= 0 && (client_pid != -1 && strcmp(req.command, "quit") != 0)) {
        printf(">> Client PID %d disconnected\n", client_pid);
    }

    close(client_fd);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: server <dirname> <max_clients>\n");
        exit(1);
    }

    dirname = argv[1];
    struct stat st = {0};
    if (stat(dirname, &st) == -1) {
        mkdir(dirname, 0700);
    }
    
    printf(">> Server Started PID %d…\n", getpid());
    printf(">> waiting for clients...\n");


    sprintf(server_fifo, "/tmp/server.%d.fifo", getpid());
    if (mkfifo(server_fifo, 0666) < 0) {
        perror("mkfifo");
        exit(1);
    }

    server_fd = open(server_fifo, O_RDWR);
    if (server_fd < 0) {
        perror("open");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while (1) {
        ssize_t num_bytes = read(server_fd, &req, sizeof(struct request));
        if (num_bytes <= 0) {
            perror("read from client");
            exit(1);
        }

        if (client_already_connected(req.pid)) {
            continue;
        }

        int max_clients = atoi(argv[2]);

        if (num_clients >= max_clients) {
            printf(">> Max clients reached. Ignoring client PID %d\n", req.pid);
            exit(1);
        }

        printf(">> Client PID %d connected as “client%d”\n", req.pid, num_clients + 1);
        client_pids[num_clients++] = req.pid;

        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = open(req.command, O_WRONLY);
        if (*client_fd_ptr < 0) {
            perror("open");
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, process_client_request, client_fd_ptr) != 0) {
            perror("pthread_create");
            continue;
        }
    }

    return 0;
}