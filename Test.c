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
#define BUFFER_SIZE 4096

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

void input_redirection_files(char ** args, int index){
    char *first_file = args[index]; // Assuming the first file is at index 1
    
    // Open the first file for appending
    int output_fd = open(first_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (output_fd == -1) {
        perror("Error opening first file");
        exit(EXIT_FAILURE);
    }

    // Iterate through input files starting from the second one
    for (int i = index+1; args[i] != NULL && strcmp(args[i], ">") != 0 && strcmp(args[i], "|") != 0; i++) {
        // Open the current input file for reading
        int input_fd = open(args[i], O_RDONLY);
        if (input_fd == -1) {
            perror("Error opening input file");
            exit(EXIT_FAILURE);
        }

        // Move to the end of the first file
        if (lseek(output_fd, 0, SEEK_END) == -1) {
            perror("Error seeking first file");
            exit(EXIT_FAILURE);
        }

        // Copy content of input file to the end of the first file
        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(input_fd, buffer, sizeof(buffer))) > 0) {
            if (write(output_fd, buffer, bytes_read) == -1) {
                perror("Error writing to first file");
                exit(EXIT_FAILURE);
            }
        }

        close(input_fd);
    }

    close(output_fd);


}

// Function Declarations
int myShell_cd(char **args);
int myShell_exit();
int myShell_execute(char **args);
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



void execute_command(char **args, int redirect_input, char *input_file, int redirect_output, char *output_file);

int myShell_execute(char **args) {
    int i = 0;
    int redirect_input = 0, redirect_output = 0, piping = 0;
    char *input_file = NULL, *output_file = NULL;
    char *pipe_input_files[10] = {NULL}; // Assuming maximum 10 input files for piping
    char *pipe_output_files[10] = {NULL}; // Assuming maximum 10 output files for piping
    int pipe_input_count = 0, pipe_output_count = 0;

    // Find input and output files
    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] == NULL) {
                printf("Missing filename after <\n");
                return 0;
            }
            input_file = args[i+1];
            redirect_input = 1;
            i++;
        } else if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                printf("Missing filename after >\n");
                return 0;
            }
            output_file = args[i + 1];
            redirect_output = 1;
            i++;
        } else if (strcmp(args[i], "|") == 0){
            if (args[i + 1] == NULL) {
                printf("Missing command after |\n");
                return 0;
            }
            piping = 1;
            i++;
        } else {
            if (piping) {
                pipe_input_files[pipe_input_count++] = args[i];
            } else {
                pipe_output_files[pipe_output_count++] = args[i];
            }
        }
        i++;
    }

    if (piping) {
        int pid;
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return 0;
        }

        pid = fork();
        if (pid == 0) {
            // Child process
            close(pipefd[0]); // Close unused read end of the pipe
            dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe
            close(pipefd[1]); // Close the write end of the pipe

            execute_command(pipe_output_files, redirect_input, input_file, 0, NULL);
        } else if (pid < 0) {
            // Forking Error
            perror("fork");
            return 0;
        }

        // Parent process
        close(pipefd[1]); // Close the write end of the pipe
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to the read end of the pipe
        close(pipefd[0]); // Close the read end of the pipe

        execute_command(pipe_input_files, 0, NULL, redirect_output, output_file);
    } else {
        execute_command(args, redirect_input, input_file, redirect_output, output_file);
    }
    return 1;
}

void execute_command(char **args, int redirect_input, char *input_file, int redirect_output, char *output_file) {
    int pid = fork();
    if (pid == 0) {
        // Child process
        if (redirect_input) {
            int input_fd = open(input_file, O_RDONLY);
            if (input_fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        if (redirect_output) {
            int output_fd = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (output_fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Forking Error
        perror("fork");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to complete
    }
}

char **expand_wildcards(char *tokens[]) {
    glob_t glob_result;
    int i, flags = 0;
    char **expanded_strings = NULL;
    size_t num_strings = 0;

    // Iterate over tokens until NULL is encountered
    for (i = 0; tokens[i] != NULL; i++) {
        // If the token contains wildcard characters
        if (strchr(tokens[i], '*') != NULL || strchr(tokens[i], '?') != NULL) {
            // Use glob to expand the wildcard pattern
            if (glob(tokens[i], flags, NULL, &glob_result) == 0) {
                // Allocate memory for expanded strings
                expanded_strings = (char **)realloc(expanded_strings, (num_strings + glob_result.gl_pathc) * sizeof(char *));
                if (expanded_strings == NULL) {
                    perror("Memory allocation failed");
                    return NULL;
                }
                // Copy expanded filenames
                for (int j = 0; j < glob_result.gl_pathc; j++) {
                    expanded_strings[num_strings++] = strdup(glob_result.gl_pathv[j]);
                    if (expanded_strings[num_strings - 1] == NULL) {
                        perror("Memory allocation failed");
                        return NULL;
                    }
                }
                // Free memory allocated by glob
                globfree(&glob_result);
            }
        } else {
            // No wildcards, just copy the token
            expanded_strings = (char **)realloc(expanded_strings, (num_strings + 1) * sizeof(char *));
            if (expanded_strings == NULL) {
                perror("Memory allocation failed");
                return NULL;
            }
            expanded_strings[num_strings++] = strdup(tokens[i]);
            if (expanded_strings[num_strings - 1] == NULL) {
                perror("Memory allocation failed");
                return NULL;
            }
        }
    }
    // Add a NULL terminator to the expanded strings array
    expanded_strings = (char **)realloc(expanded_strings, (num_strings + 1) * sizeof(char *));
    if (expanded_strings == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }
    expanded_strings[num_strings] = NULL;
    return expanded_strings;
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
    char **expanded_args = expand_wildcards(args); // Now expand_wildcards returns char **
    if (expanded_args == NULL) {
        return 1;
    }

    // Loop to check for builtin functions
    for (int i = 0; i < numBuiltin(); i++) {
        if (strcmp(expanded_args[0], builtin_cmd[i]) == 0) {
            int result = (*builtin_func[i])(expanded_args);
            free(expanded_args); // Free memory allocated by expand_wildcards
            return result;
        }
    }

    // Handle redirection
    if (myShell_execute(expanded_args)) {
        free(expanded_args); // Free memory allocated by expand_wildcards
        return 1;
    }

    // If no piping or redirection, launch command normally
    int result = myShellLaunch(expanded_args);
    free(expanded_args); // Free memory allocated by expand_wildcards
    return result;
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
