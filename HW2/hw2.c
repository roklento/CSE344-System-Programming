#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

// function prototypes
void run_command(char *cmd_line);
void run_single_command(char *cmd);
void handle_redirection(char *cmd);
void handle_pipes_and_redirection(char *cmd);
void log_child_process(pid_t pid, char *cmd);


int main() {
    char cmd_line[1024];

    while (1) {
        printf("> ");
        fgets(cmd_line, 1024, stdin);
        cmd_line[strlen(cmd_line) - 1] = '\0'; // Remove newline character

        if (strcmp(cmd_line, ":q") == 0) {
            break;
        }
        run_command(cmd_line);
    }

    return 0;
}

void run_command(char *cmd_line) {
    if (strchr(cmd_line, '|') != NULL || strchr(cmd_line, '<') != NULL || strchr(cmd_line, '>') != NULL) {
        handle_pipes_and_redirection(cmd_line);
    } else {
        run_single_command(cmd_line);
    }
}

void run_single_command(char *cmd) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        if (execl("/bin/bash", "/bin/bash", "-c", cmd, NULL) == -1) {
            perror("execl failed to execute command in child process for command");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        log_child_process(pid, cmd);
    } else {
        perror("fork failed to create child process for command");
        exit(EXIT_FAILURE);
    }
}
void handle_redirection(char *cmd) {
    char *input_file = NULL;
    char *output_file = NULL;

    char *cmd_without_redirection = strtok(strdup(cmd), "<>");
    if (strchr(cmd, '<') != NULL) {
        input_file = strtok(NULL, "<>");
    }
    if (strchr(cmd, '>') != NULL) {
        output_file = strtok(NULL, "<>");
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (input_file) {
        int input_fd = open(input_file, O_RDONLY);
        if (input_fd == -1) {
            fprintf(stderr, "Error: Cannot open input file: %s\n", input_file);
            exit(EXIT_FAILURE);
        }
        dup2(input_fd, STDIN_FILENO);
        close(input_fd);
        }

        if (output_file) {
            int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd == -1) {
                fprintf(stderr, "Error: Cannot open output file: %s\n", output_file);
                exit(EXIT_FAILURE);
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        run_single_command(cmd_without_redirection);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        log_child_process(pid, cmd_without_redirection);
    } else {
        perror("fork failed to create child process for command");
        exit(EXIT_FAILURE);
    }

    free(cmd_without_redirection);
}
void handle_pipes_and_redirection(char *cmd) {
    char *cmds[20];
    int cmd_count = 0;

    char *token = strtok(cmd, "|");
    while (token && cmd_count < 20) {
        cmds[cmd_count++] = token;
        token = strtok(NULL, "|");
    }

    int pipefds[2 * (cmd_count - 1)];
    for (int i = 0; i < cmd_count - 1; ++i) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe failed to create pipe file descriptors for command");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < cmd_count; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (i != cmd_count - 1) {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }
            if (i != 0) {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            for (int j = 0; j < 2 * (cmd_count - 1); ++j) {
                close(pipefds[j]);
            }

            if (strchr(cmds[i], '<') != NULL || strchr(cmds[i], '>') != NULL) {
                handle_redirection(cmds[i]);
            } else {
                run_single_command(cmds[i]);
            }
            
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            perror("fork failed to create child process for command");
            exit(EXIT_FAILURE);
        }
    }

    for (int j = 0; j < 2 * (cmd_count - 1); ++j) {
        close(pipefds[j]);
    }
    for (int i = 0; i < cmd_count; ++i) {
        int status;
        pid_t pid = wait(&status);
        log_child_process(pid, cmds[i]);
    }
}



void log_child_process(pid_t pid, char *cmd) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[20];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeinfo);

    char log_filename[30];
    sprintf(log_filename, "log_%s.txt", timestamp);

    FILE *log_file = fopen(log_filename, "a");
    if (log_file == NULL) {
        perror("fopen log file failed in log_child_process function");
        exit(EXIT_FAILURE);
    }

    fprintf(log_file, "PID: %d, Command: %s\n", pid, cmd);
    fclose(log_file);
}
