#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strtok() and strcmp()
#include <unistd.h> // For fork(), pid_t
#include <sys/wait.h> // For waitpid() and associated macros

int main(int argc, char *argv[]) {
    if (isatty(0)) {
        printf("the standard input is from a terminal\n");
    } else {
        printf("the standard input is NOT from a terminal\n");
    }

    if (isatty(1)) {
        printf("the standard output is to a terminal\n");
    } else {
        printf("the standard output is NOT to a terminal\n");
    }

    for (int i = 0; i < argc; i++) {
        printf("%s", argv[i]);
    }

    return 0;
}


// RUNNING INSTRUCTIONS ./attyFinder < input.txt > output.txt
