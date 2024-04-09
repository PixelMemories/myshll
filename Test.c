#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glob.h>


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
    // Loop to check for builtin functions
    for (int i = 0; i < numBuiltin(); i++) {
        if (strcmp(args[0], builtin_cmd[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    // Handle redirection
    
    // If no piping or redirection, launch command normally
    return myShellLaunch(args);
}

void execute_command(char *args[]) {
    int arg_count = 0;
    int pipe_fd[2];
    pid_t pid;

    // Parse command arguments
    // Loop to check for builtin functions
    for (int i = 0; i < numBuiltin(); i++) {
        if (strcmp(args[0], builtin_cmd[i]) == 0) {
            execShell(args);
            return;
        }
    }
    while (args[arg_count] != NULL) {
        if (strcmp(args[arg_count], "|") == 0) { // Handle pipe
            args[arg_count] = NULL;
            arg_count = 0;

            pipe(pipe_fd);

            if ((pid = fork()) == 0) { // Child process
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid < 0) { // Fork error
                perror("fork");
                exit(EXIT_FAILURE);
            } else { // Parent process
                close(pipe_fd[1]);
                wait(NULL);
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[0]);
            }
        } else if (strcmp(args[arg_count], ">") == 0) { // Handle redirect output
            char *filename = args[arg_count + 1];
            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[arg_count] = NULL; // Nullify '>' and filename
            args[arg_count + 1] = NULL;
        } else if (strcmp(args[arg_count], "<") == 0) { // Handle redirect input
            char *filename = args[arg_count + 1];
            int fd = open(filename, O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[arg_count] = NULL; // Nullify '<' and filename
            args[arg_count + 1] = NULL;
        } else { // Handle command arguments
            arg_count++;
        }
    }

    if ((pid = fork()) == 0) { // Child process
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) { // Fork error
        perror("fork");
        exit(EXIT_FAILURE);
    } else { // Parent process
        wait(NULL);
    }
}


// When myShell is called Interactively
int myShellInteract() {
    char *line;
    char **args;
    while (QUIT == 0) {
        printf("%s> ", SHELL_NAME);
        line = readLine();
        args = splitLine(line);
        execute_command(args);
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
