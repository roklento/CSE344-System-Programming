#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

char *directory;

void handle_sigint(int sig) {
    printf("Received SIGINT. Shutting down server.\n");
    exit(0);
}

void send_file(int client_socket, char *file_name) {
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        perror("File open error");
        return;
    }

    char file_name_buffer[256];
    strncpy(file_name_buffer, file_name, sizeof(file_name_buffer));
    send(client_socket, file_name_buffer, sizeof(file_name_buffer), 0);

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    send(client_socket, &file_size, sizeof(file_size), 0);

    char buffer[256];
    size_t n;
    while ((n = fread(buffer, sizeof(char), sizeof(buffer), file)) > 0) {
        if (send(client_socket, buffer, n, 0) < 0) {
            perror("Failed to send file");
            fclose(file);
            return;
        }
    }

    fclose(file);
}



void send_directory_contents(int client_socket, const char *directory) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    if ((dir = opendir(directory)) == NULL) {
        perror("Failed to open directory");
        return;
    }

    chdir(directory);

    while ((entry = readdir(dir)) != NULL) {
        stat(entry->d_name, &file_stat);
        if (S_ISREG(file_stat.st_mode)) {
            printf("Sending file: %s\n", entry->d_name);
            send_file(client_socket, entry->d_name);
        }
    }

    chdir("..");
    closedir(dir);
}

void *client_handler(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    send_directory_contents(client_socket, directory);
    close(client_socket);
    free(socket_desc);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: ./server <directory> <threadPoolSize> <portnumber>\n");
        return 1;
    }

    directory = argv[1];
    int threadPoolSize = atoi(argv[2]);
    int portnumber = atoi(argv[3]);

    struct stat st = {0};

    if (stat(directory, &st) == -1) {
        mkdir(directory, 0777);
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portnumber);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));
    listen(server_socket, threadPoolSize);

    signal(SIGINT, handle_sigint);

    printf("Server is listening on port %d\n", portnumber);

    pthread_t thread_id;
    while (1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &addr_size);
        printf("Connection accepted\n");

        int *new_sock;
        new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        pthread_create(&thread_id, NULL, client_handler, (void *)new_sock);
    }

    close(server_socket);

    return 0;
}