#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


char SHELL_NAME[50] = "myShell";
int QUIT = 0;

#define MAX_COMMAND_LENGTH 1024

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
int myShell_redirect_input_output(char **args);
int myShell_pwd();
int myShell_which(char **args);


// Definitions
char *builtin_cmd[] = {"cd", "exit", "pwd", "which"};

int (*builtin_func[])(char **) = {&myShell_cd, &myShell_exit, &myShell_pwd, &myShell_which};

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
    printf("Exiting Shell... See you soon!!\n");
    QUIT = 1;
    return 0;
}

int myShell_pwd(){
    // dynamic array?????
    char pwd[1024]; 

    if (getcwd(pwd, sizeof(pwd)) != NULL) {
        printf("%s\n", pwd);
    } else {
        perror("pwd error");
        return 1;
    }

    return 0;
}

int myShell_which(char **args) {
    int argc = 0;

    // Count the number of arguments
    while (args[argc] != NULL) {
        argc++;
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program_name>\n", args[0]);
        return 1; // Return error code 1 for wrong number of arguments
    }

    char *program_name = args[1];
    
    /*
    if(strcmp(program_name, "exit") == 0 || strcmp(program_name, "cd") == 0 || strcmp(program_name, "pwd") == 0 || strcmp(program_name, "which") == 0) {
        printf("Built-in Command does not have Path\n");
        return 1; 
    }
    */
    

    char *path = getenv("PATH");

    if (path == NULL) {
        fprintf(stderr, "Error: PATH environment variable is not set.\n");
        return 1;
    }

    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 1;
    }

    char *token;
    char file_path[1024];

    token = strtok(path_copy, ":");
    while (token != NULL) {
        snprintf(file_path, 1024, "%s/%s", token, program_name);
        if (access(file_path, X_OK) == 0) {
            printf("%s\n", file_path);
            free(path_copy);
            return 0;
        }
        token = strtok(NULL, ":");
    }

    free(path_copy);

    fprintf(stderr, "%s: program not found\n", args[1]);
    return 1;
}



int myShell_redirect_input_output(char **args) {
    int i = 0;
    int redirect_input = 0, redirect_output = 0;
    char *input_file = NULL, *output_file = NULL;

    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {
            // Check if a filename is provided after '<'
            if (args[i + 1] == NULL) {
                printf("Missing filename after <\n");
                return 1;
            }
            input_file = args[i + 1];
            redirect_input = 1;
        } else if (strcmp(args[i], ">") == 0) {
            // Check if a filename is provided after '>'
            if (args[i + 1] == NULL) {
                printf("Missing filename after >\n");
                return 1;
            }
            output_file = args[i + 1];
            redirect_output = 1;
        }
        i++;
    }

    int pid = fork();

    if (pid == 0) {
        // Child process
        if (redirect_input) {
            int redirect_input_fd = open(input_file, O_RDONLY);
            if (redirect_input_fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            // Redirect standard input to the file
            dup2(redirect_input_fd, STDIN_FILENO);
            close(redirect_input_fd);
        }

        if (redirect_output) {
            int redirect_output_fd = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (redirect_output_fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            // Redirect standard output to the file
            dup2(redirect_output_fd, STDOUT_FILENO);
            close(redirect_output_fd);
        }

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

    return 0;
}


// Function to check for pipes and execute accordingly
int myShell_pipe(char **args) {
    int pipefd[2]; // file descriptors for the pipe
    int pipe_pos = -1; // Position of the pipe character in args
    pid_t pid1, pid2;

    // Find the position of the pipe character, if present
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_pos = i;
            break;
        }
    }

    // No pipe found, return indicating that no pipe handling is necessary
    if (pipe_pos == -1) {
        return 0;
    }

    // Create a pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Fork the first child process
    pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {
        // Child process 1: executes the command before the pipe
        // Redirect stdout to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); // Close unused read end
        close(pipefd[1]); // Close write end after dup2

        // Null-terminate the args array at the position of the pipe
        args[pipe_pos] = NULL;
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    // Fork the second child process
    pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {
        // Child process 2: executes the command after the pipe
        // Redirect stdin to the read end of the pipe
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]); // Close unused write end
        close(pipefd[0]); // Close read end after dup2

        // Execute the command following the pipe
        execvp(args[pipe_pos + 1], &args[pipe_pos + 1]);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    // Close pipe ends in the parent
    close(pipefd[0]);
    close(pipefd[1]);

    // Wait for both child processes to complete
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    // Return indicating that pipe handling was done
    return 1;
}

int match(const char *pattern, const char *filename) {
    while (*pattern && *filename) {
        if (*pattern == '*') {
            while (*filename) {
                if (match(pattern + 1, filename))
                    return 1;
                filename++;
            }
            return *pattern == '\0';
        } else if (*pattern != '?' && *pattern != *filename) {
            return 0;
        }
        pattern++;
        filename++;
    }
    return !*pattern && !*filename;
}

int expand_wildcards(char **args) {
    struct dirent *entry;
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        for (int i = 1; args[i] != NULL; i++) {
            if (strchr(args[i], '*') != NULL) {
                if (match(args[i], entry->d_name)) {
                    args[i] = strdup(entry->d_name);
                }
            }
        }
    }
    closedir(dir);
    printf("I got here\n");
    return 1;
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
    if (args[0] == NULL) {
        return 1;
    }
    // Handle piping
    if (myShell_pipe(args)) {
        return 1; // Pipe handled
    }
    // Loop to check for builtin functions
    for (int i = 0; i < numBuiltin(); i++) {
        if (strcmp(args[0], builtin_cmd[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    // Handle redirection
    if (myShell_redirect_input_output(args)) {
        return 1;
    }
    // Handle Wildcards
    expand_wildcards(args);
    
    // If no piping or redirection, launch command normally
    return myShellLaunch(args);
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
int myShellBatch(FILE *filename) {
    char line[MAX_COMMAND_LENGTH];
    char **args;
    if (filename == NULL) {
        printf("\nUnable to open file.");
        return 1;
    } else {
        printf("\nFile Opened. Parsing. Parsed commands displayed first.");
        while (fgets(line, sizeof(line), filename) != NULL) {
            printf("\n%s", line);
            args = splitLine(line);
            execShell(args);
        }
    }
    free(args);
    fclose(filename);
    return 1;
}

int BMCheck(int argc, char *argv[]) {
    // Check if there are command-line arguments
    if (argc > 1) {
        return 1; // Running in batch mode
    }

    // Check if input is being piped
    if (!isatty(fileno(stdin))) {
        return 1; // Running in batch mode
    }

    return 0; // Running in interactive mode
}

int main(int argc, char **argv) {
    // Parsing commands Interactive mode or Script Mode
    if (BMCheck(argc, argv)) {
        if (argc > 1) {
            printf("Running in batch mode with file: %s\n", argv[1]);
            FILE *file = fopen(argv[1], "r");
            if (file == NULL) {
                perror("Error opening file");
                return 1;
            }
            myShellBatch(file);
        } else {
            printf("Running in batch mode with piped input\n");
            myShellBatch(stdin);
        }

    } else {
        printf("Running in interactive mode\n");
        printf("Welcome to your personal Shell!!\n");
        myShellInteract();

    }
    
    // Exit the Shell
    return EXIT_SUCCESS;
}
