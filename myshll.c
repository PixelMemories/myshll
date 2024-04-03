#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

char SHELL_NAME[50] = "myShell";
int QUIT = 0;

// Function to read a line from command into the buffer
char *readLine() {
    char *line = (char *)malloc(sizeof(char) * 1024);
    char c;
    int pos = 0, bufsize = 1024;
    if (!line) {
        printf("\nBuffer Allocation Error.");
        exit(EXIT_FAILURE);
    }
    while (1) {
        c = getchar();
        if (c == EOF || c == '\n') {
            line[pos] = '\0';
            return line;
        } else {
            line[pos] = c;
        }
        pos++;
        if (pos >= bufsize) {
            bufsize += 1024;
            line = realloc(line, sizeof(char) * bufsize);
            if (!line) {
                printf("\nBuffer Allocation Error.");
                exit(EXIT_FAILURE);
            }
        }
    }
}

// Function to split a line into constituent commands
char **splitLine(char *line) {
    char **tokens = (char **)malloc(sizeof(char *) * 64);
    char *token;
    char delim[10] = " \t\n\r\a";
    int pos = 0, bufsize = 64;
    if (!tokens) {
        printf("\nBuffer Allocation Error.");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, delim);
    while (token != NULL) {
        tokens[pos] = token;
        pos++;
        if (pos >= bufsize) {
            bufsize += 64;
            line = realloc(line, bufsize * sizeof(char *));
            if (!line) {
                printf("\nBuffer Allocation Error.");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, delim);
    }
    tokens[pos] = NULL;
    return tokens;
}

// Function Declarations
int myShell_cd(char **args);
int myShell_exit();
int myShell_redirect_output(char **args);

// Definitions
char *builtin_cmd[] = {"cd", "exit"};

int (*builtin_func[])(char **) = {&myShell_cd, &myShell_exit};

int numBuiltin() {
    return sizeof(builtin_cmd) / sizeof(char *);
}

// Builtin command definitions
int myShell_cd(char **args) {
    if (args[1] == NULL) {
        printf("myShell: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("myShell: ");
        }
    }
    return 1;
}

int myShell_exit() {
    QUIT = 1;
    return 0;
}

int myShell_redirect_output(char **args) {
    int i = 0;
    // Check if the command is the shell prompt itself
    if (strcmp(args[0], SHELL_NAME) == 0) {
        return 0; // Skip redirection
    }
    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
            // Check if a filename is provided after '>'
            if (args[i + 1] == NULL) {
                printf("Missing filename after >\n");
                return 1;
            }

            int pid = fork();

            if (pid == 0) {
                // Inside the child process
                // Open the file specified after '>'
                int redirect_output_fd = open(args[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                if (redirect_output_fd == -1) {
                    perror("open");
                    return 1;
                }
                // Redirect standard output to the file
                dup2(redirect_output_fd, STDOUT_FILENO);
                close(redirect_output_fd);

                // Execute the external command
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid < 0) {
                // Forking Error
                perror("fork");
                return 1;
            } else {
                // Inside the parent process
                int status;
                waitpid(pid, &status, 0); // Wait for the child process to complete
                return 1;
            }
        }
        i++;
    }
    return 0;
}

int myShellLaunch(char **args) {
    pid_t pid, wpid;
    int status;
    pid = fork();
    if (pid == 0) {
        // The Child Process
        if (execvp(args[0], args) == -1) {
            perror("myShell: ");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        // Forking Error
        perror("myShell: ");
    } else {
        // The Parent Process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

// Function to execute command from terminal
int execShell(char **args) {
    int ret;
    if (args[0] == NULL) {
        // Empty command
        return 1;
    }
    // Loop to check for builtin functions
    for (int i = 0; i < numBuiltin(); i++) {
        if (strcmp(args[0], builtin_cmd[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    // Check for redirection
    if (myShell_redirect_output(args)) {
        return 1;
    }
    ret = myShellLaunch(args);
    return ret;
}

// When myShell is called Interactively
int myShellInteract() {
    char *line;
    char **args;
    while (QUIT == 0) {
        printf("%s> ", SHELL_NAME);
        line = readLine();
        args = splitLine(line);
        //Do Shell
        execShell(args);
        free(line);
        free(args);
    }
    return 1;
}

// When myShell is called with a Script as Argument
int myShellScript(char filename[100]) {
    printf("Received Script. Opening %s", filename);
    FILE *fptr;
    char line[200];
    char **args;
    fptr = fopen(filename, "r");
    if (fptr == NULL) {
        printf("\nUnable to open file.");
        return 1;
    } else {
        printf("\nFile Opened. Parsing. Parsed commands displayed first.");
        while (fgets(line, sizeof(line), fptr) != NULL) {
            printf("\n%s", line);
            args = splitLine(line);
            execShell(args);
        }
    }
    free(args);
    fclose(fptr);
    return 1;
}

int main(int argc, char **argv) {
    // Parsing commands Interactive mode or Script Mode
    if (isatty(0) && argc == 1) {
        //interactive
        myShellInteract();
    } else if (argc > 2) {
        printf("\nInvalid Number of Arguments");
    } else {
        //Other mode.
        myShellScript(argv[1]);
    }
    // Exit the Shell
    return EXIT_SUCCESS;
}
