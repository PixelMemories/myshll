
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <glob.h>

#define MAX_ARGS 64
#define MAX_COMMANDS 64
#define MAX_PATH 1024

char *builtin_commands[] = {"cd", "pwd", "which", "exit"};
char *builtin_paths[] = {"/bin/", "/usr/bin/", "/usr/local/bin/"};

void execute_command(char *command);

int is_builtin(char *command) {
    for (int i = 0; i < sizeof(builtin_commands) / sizeof(char *); i++) {
        if (strcmp(command, builtin_commands[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void cd_command(char *args[]) {
    if (args[1] == NULL) {
        fprintf(stderr, "Usage: cd <directory>\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("chdir");
        }
    }
}

void pwd_command() {
    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd");
    }
}

void which_command(char *args[]) {
    if (args[1] == NULL) {
        fprintf(stderr, "Usage: which <command>\n");
    } else {
        char command_path[MAX_PATH];
        int found = 0;
        for (int i = 0; i < sizeof(builtin_paths) / sizeof(char *); i++) {
            strcpy(command_path, builtin_paths[i]);
            strcat(command_path, args[1]);
            if (access(command_path, X_OK) == 0) {
                printf("%s\n", command_path);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("%s: command not found\n", args[1]);
        }
    }
}

void exit_command(char *args[]) {
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s ", args[i]);
    }
    printf("\n");
    exit(EXIT_SUCCESS);
}

void execute_builtin_command(char *args[]) {
    if (strcmp(args[0], "cd") == 0) {
        cd_command(args);
    } else if (strcmp(args[0], "pwd") == 0) {
        pwd_command();
    } else if (strcmp(args[0], "which") == 0) {
        which_command(args);
    } else if (strcmp(args[0], "exit") == 0) {
        exit_command(args);
    }
}

void execute_command(char *command) {
    char *commands[MAX_COMMANDS];
    int i = 0;

    commands[i] = strtok(command, "|");
    while (commands[i] != NULL && i < MAX_COMMANDS - 1) {
        i++;
        commands[i] = strtok(NULL, "|");
    }
    commands[i] = NULL;

    if (commands[0] == NULL) {
        return;
    }

    int pipes[MAX_COMMANDS - 1][2];
    int fd_in = 0; // File descriptor for input

    for (i = 0; commands[i] != NULL; i++) {
        char *args[MAX_ARGS];
        int j = 0;

        args[j] = strtok(commands[i], " \t\n");
        while (args[j] != NULL && j < MAX_ARGS - 1) {
            j++;
            args[j] = strtok(NULL, " \t\n");
        }
        args[j] = NULL;

        for (j = 0; args[j] != NULL; j++) {
            glob_t globbuf;
            memset(&globbuf, 0, sizeof(glob_t));
            if (strchr(args[j], '*') != NULL || strchr(args[j], '?') != NULL) {
                if (glob(args[j], GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) == 0) {
                    for (int k = 0; k < globbuf.gl_pathc; k++) {
                        args[j] = globbuf.gl_pathv[k];
                        execute_builtin_command(args);
                    }
                    globfree(&globbuf);
                }
            }
        }

        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (i == 0) {
                // First command
                dup2(pipes[i][1], 1); // Redirect output to the write end of the first pipe
            } else if (commands[i + 1] != NULL) {
                // Intermediate command
                dup2(fd_in, 0); // Redirect input from the read end of the previous pipe
                dup2(pipes[i][1], 1); // Redirect output to the write end of the current pipe
            } else {
                // Last command
                dup2(fd_in, 0); // Redirect input from the read end of the previous pipe
                // Check for output redirection
                for (j = 0; args[j] != NULL; j++) {
                    if (strcmp(args[j], ">") == 0) {
                        args[j] = NULL;
                        int fd_out = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                        if (fd_out == -1) {
                            perror("open");
                            exit(EXIT_FAILURE);
                        }
                        dup2(fd_out, 1); // Redirect output to the specified file
                        close(fd_out);
                    }
                }
            }
            // Close all pipe ends
            for (int k = 0; k < i; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            execute_builtin_command(args);
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            // Error forking
            perror("fork");
            exit(EXIT_FAILURE);
        }

        close(pipes[i][1]); // Close write end of the pipe
        fd_in = pipes[i][0]; // Set the input for the next command
    }

    for (int j = 0; j < i; j++) {
        close(pipes[j][0]); // Close read end of the pipe
        close(pipes[j][1]); // Close write end of the pipe
    }

    for (int j = 0; j < i; j++) {
        wait(NULL); // Wait for all child processes to complete
    }
}

int main() {
    char command[MAX_PATH];

    while (1) {
        printf("$ ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        // Removing trailing newline character
        command[strcspn(command, "\n")] = 0;

        execute_command(command);
    }

    return 0;
}
