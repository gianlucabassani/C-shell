#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define CMD_AVAILABLE 5

static const char *builtin_cmd[CMD_AVAILABLE] = {"exit", "echo", "type", "pwd", "cd"};


// Function to read input from stdin
void read_command(char *input) {
    if (fgets(input, MAX_CMD_LEN, stdin) == NULL) {
        // Handle EOF (Ctrl+D)
        printf("\n");
        exit(0);
    }
    
    // Remove newline character
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }
}

// Function to find executable in PATH
char* find_executable(const char *command) {
    const char *path_env = getenv("PATH");
    if (path_env == NULL) {
        return NULL;
    }
    char path_copy[MAX_CMD_LEN];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1); 
    path_copy[sizeof(path_copy) - 1] = '\0'; // add null terminator
    
    char *dir_path = strtok(path_copy, ":"); // divide PATH by ':'
    while (dir_path != NULL) {
        char full_path[MAX_CMD_LEN];
        // sprintf is used to format and store a series of characters and values in the array full_path
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, command);
        
        // access checks if the file exists and is executable.
        if (access(full_path, X_OK) == 0) { // X_OK = constant for execute permissions
            
            // malloc allocates memory for the found path to the heap for later use
            char *result = malloc(strlen(full_path) + 1); // +1 for null terminator
            if (result != NULL) {
                strcpy(result, full_path); // copy full path to allocated memory (for launching the command later)
            }
            return result;
        }
        dir_path = strtok(NULL, ":"); // continue to the next directory in PATH
    }
    
    return NULL;
}

// Function to tokenize command into arguments 
int parse_command(char *input, char **argv) {
    int argc = 0;
    char *token = strtok(input, " \t"); // divide input by spaces and tabs
    while (token != NULL && argc < MAX_ARGS - 1) { // while there are tokens (< max args)
        argv[argc++] = token; // add token to argv and increment argc (argument count)
        //printf("argv[%d]: %s\n", argc-1, token); // debug printer for arguments
        token = strtok(NULL, " \t"); 
    }
    argv[argc] = NULL; 
    
    return argc;
}

// Function to check if command is a builtin (used in type command)
int is_builtin(const char *command) {
    for (int i = 0; i < CMD_AVAILABLE; i++) {
        if (strcmp(command,builtin_cmd[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Function to execute builtin commands
int execute_builtin(char **argv) {
    
    // check if the command is "exit" and the second argument is "0":
    if (strcmp(argv[0], "exit") == 0 && argv[1] && strcmp(argv[1], "0") == 0) {
        exit(0);
    }

    // === "echo" command ===
    else if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; argv[i] != NULL; i++) { // if the next argument is not NULL (last argument)
            printf("%s", argv[i]); // append every argument after "echo"
            if (argv[i + 1] != NULL) { 
                printf(" "); // add space between args
            }     
        }
        printf("\n");
    }

    // === "type" command ===
    else if (strcmp(argv[0], "type") == 0) {
        if (argv[1] == NULL) {
            printf("type: missing argument\n");
            return 1;
        }

        // Check if it's a built-in
        if (is_builtin(argv[1])) {
            printf("%s is a shell builtin\n", argv[1]);
        } else {
            // Search in PATH
            char *full_path = find_executable(argv[1]); // return full path or NULL
            if (full_path != NULL) {
                printf("%s is %s\n", argv[1], full_path);
                free(full_path); // free allocated memory
            } else {
                printf("%s is not found\n", argv[1]);
            }
        }
    }
    
    // === "pwd" command ===
    else if (strcmp(argv[0], "pwd") == 0) {
        char *cwd = getcwd(NULL, 0);
        // char cwd[PATH_MAX]; // alternative way with static buffer
        if (cwd != NULL) {
            printf("%s\n", cwd);
            free(cwd); // free memory with the path
        } else {
            perror("getcwd failed");
            return 1;
        }
    }
    
    // === "cd" command ===
    else if (strcmp(argv[0], "cd") == 0) {
        char *target_dir = argv[1];
        if (target_dir == NULL) {
            target_dir = getenv("HOME");
            if (target_dir == NULL) {
                perror("cd: HOME not set");
                return 1;
            }
        }
        if (chdir(target_dir) != 0) {
            perror("cd failed");
            return 1;
        }
        return 0;

    }
    return 0;
}

// Function to execute external commands
int execute_external(char **argv) {
    char *full_path = find_executable(argv[0]);
    if (full_path == NULL) {
        printf("%s: command not found\n", argv[0]);
        return 1;
    }

    // Create a new process (pid_t = process ID type)
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        free(full_path);
        return 1;
    }
    else if (pid == 0) {
        // Child process
        extern char **environ; // extern = external variable (used to access environment variables)
        if (execve(full_path, argv, environ) == -1) {
            perror("execve failed");
            exit(1);
        }
    }
    else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) { // wait and sync with existing child process
            perror("waitpid failed");
        }
        free(full_path);
        
        // Return exit status of the child
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return 1;
    }
    
    return 0;
}



int main() {
    // Disable buffering for immediate output
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    while (1) {
        printf("$ ");

        char input[MAX_CMD_LEN];
        char *argv[MAX_ARGS];

        // Read command from stdin
        read_command(input);

        // handle empty input
        if (strlen(input) == 0) {
            continue;
        }

        // Tokenize command into arguments
        int argc = parse_command(input, argv);
        if (argc == 0) {
            continue;
        }

        // Execute command
        if (is_builtin(argv[0])) {
            execute_builtin(argv);
        } else {
            execute_external(argv);
        }
    }

    return 0;
}