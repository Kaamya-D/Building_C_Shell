

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

// Define a fallback for PATH_MAX if not defined
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Define a fallback for HOST_NAME_MAX if not defined
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

void display_prompt(const char *home_dir) {
    char cwd[PATH_MAX];
    char hostname[HOST_NAME_MAX];
    char *username = getenv("USER");

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname failed");
        return;
    }

    // Display relative path if within home directory
    if (strncmp(cwd, home_dir, strlen(home_dir)) == 0) {
        printf("<%s@%s:~%s> ", username, hostname, cwd + strlen(home_dir));
    } else {
        printf("<%s@%s:%s> ", username, hostname, cwd);
    }
}

void execute_command(char *command) {
    char *args[256];
    int i = 0;

    // Tokenize command into arguments
    char *token = strtok(command, " \t\n");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL; // Null-terminate the arguments array

    // Handle built-in cd command
    if (strcmp(args[0], "cd") == 0) {
        char *target_dir;

        if (args[1] == NULL || strcmp(args[1], "~") == 0) {
            target_dir = getenv("HOME"); // Default to home directory if no argument or "~"
        } else {
            target_dir = args[1]; // Use provided argument
        }

        if (chdir(target_dir) != 0) {
            perror("cd failed");
        }
        return; // No need to fork for built-in commands
    }

    pid_t pid = fork();

    if (pid == 0) { // Child process
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "ERROR: '%s' is not a valid command\n", args[0]);
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) { // Parent process
        if (strcmp(args[i - 1], "&") == 0) { // Check for background job
            printf("[%d] %d\n", getpid(), pid); // Print process ID
            args[i - 1] = NULL; // Remove '&' before execution
        } else {
            waitpid(pid, NULL, 0); // Wait for the child to finish
        }
    } else {
        perror("fork failed");
    }
}

void monitor_background_processes() {
    int status;
    pid_t pid;

    // Non-blocking wait to check background processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Background process with PID %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Background process with PID %d terminated by signal %d\n", pid, WTERMSIG(status));
        }
    }
}

int main() {
    char home_dir[PATH_MAX];

    // Initialize home directory to the current directory
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
        perror("getcwd failed");
        return EXIT_FAILURE;
    }

    while (1) {
        monitor_background_processes(); // Check for background processes
        display_prompt(home_dir);       // Display the prompt
        
        char input[1024];               // Buffer to store user input

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\nExiting shell.\n");
            break;
        }

        // Remove trailing newline character from input
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            printf("Exiting shell.\n");
            break;
        }

        // Tokenize input based on ';'
        char *command = strtok(input, ";");
        while (command != NULL) {
            // Trim leading and trailing spaces
            command += strspn(command, " \t");
            if (*command != '\0') {
                execute_command(command);
            }
            command = strtok(NULL, ";");
        }
    }

    return EXIT_SUCCESS;
}
