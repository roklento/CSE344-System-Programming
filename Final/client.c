#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

void handle_sigint(int sig) {
    printf("Received SIGINT. Closing client.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./client <directory> <portnumber>\n");
        return 1;
    }

    char *dirName = argv[1];
    int portnumber = atoi(argv[2]);

    struct stat st = {0};

    if (stat(dirName, &st) == -1) {
        if (mkdir(dirName, 0700) != 0) {
            perror("Directory creation error");
            return 1;
        }
    }
    if (chdir(dirName) != 0) {
        perror("Failed to change directory");
        return 1;
    }

    signal(SIGINT, handle_sigint);

    while(1) {
        int client_socket;
        struct sockaddr_in server_addr;
        client_socket = socket(AF_INET, SOCK_STREAM, 0);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(portnumber);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            perror("Connection error");
            close(client_socket);
            sleep(10);
            continue;
        }

        printf("Connected to server\n");

        char file_name[256];
        long file_size;
        char buffer[256];

        while (1) {
            if (recv(client_socket, file_name, sizeof(file_name), 0) <= 0) break;
            if (recv(client_socket, &file_size, sizeof(file_size), 0) <= 0) break;
            FILE *file = fopen(file_name, "wb");
            if (file == NULL) {
                perror("Error opening file for writing");
                continue;
            }
            long remaining_bytes = file_size;
            while (remaining_bytes > 0) {
                size_t n = recv(client_socket, buffer, sizeof(buffer), 0);
                if (n <= 0) break;
                fwrite(buffer, sizeof(char), n, file);
                remaining_bytes -= n;
            }

            fclose(file);
            printf("File received: %s\n", file_name);
        }

        close(client_socket);
        printf("Connection closed. Reconnecting in 10 seconds...\n");

        sleep(10);
    }

    return 0;
}
