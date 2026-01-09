#include <stdio.h> // standard I/O
#include <stdlib.h> // memory allocation
#include <string.h> // strings functions
#include <unistd.h> // posix api (fork, execve, ..)
#include <sys/types.h> // data types for syscalls
#include <sys/wait.h> // wait for process to change state
#include <limits.h> // system limits
#include <fcntl.h> // file control options (open, o_wronly, ..)
#include <readline/readline.h> // gnu readline lib
#include <readline/history.h> // command history
#include <dirent.h> // directory ops (opendir, readdir, ..)

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define CMD_AVAILABLE 6
#define MAX_PIPELINE_CMDS 32 


static const char *builtin_cmd[CMD_AVAILABLE] = {"exit", "echo", "type", "pwd", "cd", "history"};
static int last_appended_count = 0;
static int initial_history_length = 0;


// Prototypes
void load_history_from_histfile(void);
void save_history_to_histfile(void);

/*
static = variable only visible in this file
const char * = array of pointers to const chars 
*/

// Helper function to duplicate a string (used in completion)
char *dupstr(char *s) {
    char *r;
    r = malloc(strlen(s) + 1); // allocate memory +1 for null temrinator
    strcpy(r, s); // copy string s to new memory r
    return (r); // return pointer to new string
}
// the caller must free() later

int apply_redirection(const char *filename, int target_fd, int flags) {
    if (filename == NULL) return 0;

    int fd = open(filename, flags, 0644);
    if (fd == -1) {
        perror("open failed");
        return -1;
    }
    
    if (dup2(fd, target_fd) == -1) {
        perror("dup2 failed");
        close(fd);
        return -1;
    }
    close(fd);
    return 0; // Success
}

char *command_generator(const char *text, int state) {
    static int list_index, len;
    const char *name;
    
    // Static variables to maintain state across calls
    static char *path = NULL;
    static char *dir = NULL;
    static DIR *dirp = NULL;
    static struct dirent *dp = NULL;

    /*
    - Normal local variables are destroyed when fun returns
    - static local variables persists between fun calls, this consets to remember where we left off 
    */

    // First call (state == 0)
    if (!state) {
        list_index = 0;
        len = strlen(text);
        
        // Clean up and get fresh PATH
        if (path) {
            free(path);
        }
        
        const char *path_env = getenv("PATH"); 
        if (path_env) {
            path = strdup(path_env); // make a copy we can modify
            dir = strtok(path, ":"); // tokenize path by ':'
        } else {
            path = NULL;
            dir = NULL;
        }
        
        // Close any open directory
        if (dirp) {
            closedir(dirp);
        }
        dirp = NULL;
    }

    // First, check built-in commands
    while (list_index < CMD_AVAILABLE) {
        name = builtin_cmd[list_index];
        list_index++;
        
        if (strncmp(name, text, len) == 0) { // does it match?
            return dupstr((char *)name); // return a copy
        }
    }

    // Then scan through PATH directories
    while (dir) {
        if (!dirp) {
            dirp = opendir(dir); // open dir for reading
        }
        
        if (dirp) {
            while ((dp = readdir(dirp)) != NULL) { // read each entry
                if (strncmp(dp->d_name, text, len) == 0) { 
                    char fpath[MAX_CMD_LEN]; 
                    snprintf(fpath, sizeof(fpath), "%s/%s", dir, dp->d_name);
                    
                    // Check if file is executable
                    if (access(fpath, X_OK) == 0) { // check if file has execute permissions
                        return dupstr(dp->d_name); // match found
                    }
                }
            }
            closedir(dirp); // done with this dir
            dirp = NULL;
        }
        dir = strtok(NULL, ":"); // move to the next dir
    }
    
    return NULL; // no more matches
}


char **builtin_completition(const char *text, int start, int end) {
    char **matches = NULL;

    // only complete if beginning of line (complete only command itself)
    if (start == 0) {
        matches = rl_completion_matches(text, command_generator);
    }
    return matches; // return array of matches
}

void initialize_readline() {
    // Set custom completion function
    rl_attempted_completion_function = builtin_completition;

    // Don't append space automatically (use space in generator)
    rl_completion_append_character = '\0';

    // custom character appending
    rl_completion_suppress_append = 0;
}

char *read_command_readline() {
    char prompt[MAX_CMD_LEN];
    
    // Get username
    char *username = getenv("USER");
    if (username == NULL) username = "user";
    
    // Get hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }
    
    // Get current directory
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        perror("getcwd failed");
        cwd = strdup("/");
    }
    
    // Shorten home directory to ~
    char *home = getenv("HOME");
    char display_path[MAX_CMD_LEN];
    if (home != NULL && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home));
    } else {
        snprintf(display_path, sizeof(display_path), "%s", cwd);
    }
    
    // Get time
    char timebuf[6];

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%H:%M", t);

    // Format: [12:34] user@hostname:~/path$ 
    snprintf(prompt, sizeof(prompt), 
            "\033[33m[%s]\033[0m "         // time (yellow)
            "\033[31m\033[1m%s\033[0m"    // username (red)
            "\033[33m\033[1m@\033[0m"      // @ separator
            "\033[31m\033[1m%s\033[0m"     // hostname (red)
            "\033[33m:\033[0m"             // : separator
            "\033[91m%s\033[0m"           // path (bright red)
            "\033[33m$ \033[0m",           // prompt symbol (yellow),

            timebuf, username, hostname, display_path);
    
    char *input = readline(prompt);
    free(cwd);
    
    // Handle EOF (Ctrl+D)
    save_history_to_histfile();
    if (input == NULL) {
        printf("\n");
        exit(0);
    }

    // Add to history if not empty
    if (input && *input) {
        add_history(input); 
    }
    return input;
 
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
    // strncpy don't guarantee null termination so for safety we add it manually

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

// Function to tokenize command into arguments (quotes & escapes handled)
int parse_command(char *input, char **argv) {
    int argc = 0; // arg count
    int i = 0; // input index
    char current_arg[MAX_CMD_LEN]; // buffer for building argument
    int arg_pos = 0; // index of current_arg
    int in_single_quote = 0;
    int in_double_quote = 0;

    while (input[i] != '\0' && argc < MAX_ARGS - 1) {
        char ch = input[i];

        if (in_single_quote) {
            if (ch == '\'') {
                in_single_quote = 0; // close single quote
            } else {
                current_arg[arg_pos++] = ch; // add char to current arg (no escape)
            }
        }
        else if (in_double_quote) {
            if (ch == '"') {
                in_double_quote = 0; // close double quote
            } else if (ch == '\\') {
                // handle escape sequences in double quotes
                char next = input[i + 1];
                if (next == '"' || next == '\\' || next == '$' || next == '`' || next == '\n') { // char escapable
                    current_arg[arg_pos++] = next; // add escaped char
                    i++; // skip next char
                } else {
                    current_arg[arg_pos++] = ch; // add backslash as is
                }
            } else {
                current_arg[arg_pos++] = ch; // add char to current arg
            }
        }
        else { // outside quotes
            if (ch == '\'') { // open single quote
                in_single_quote = 1; 
            } else if (ch == '"') { // open double quote
                in_double_quote = 1;
            } else if (ch == '\\') { // handle escape outside quotes
                if (input[i + 1] != '\0') { // ensure not end of string
                    current_arg[arg_pos++] = input[i + 1]; // add next char
                    i++; 
                }
            } else if (ch == ' ' || ch == '\t') { // check for space or tab
                if (arg_pos > 0) { // fine argomento
                    current_arg[arg_pos] = '\0'; // null terminate current arg
                    argv[argc] = malloc(arg_pos + 1); // use malloc to allocate memory for the argument (otherwise it will be lost after the function ends)
                    if (argv[argc] != NULL) { 
                        strcpy(argv[argc], current_arg); // copy current arg (char) to argv (array of strings)
                        argc++; 
                    }
                    arg_pos = 0; // reset for next arg
                }
                // skip consecutive spaces/tabs
                while (input[i + 1] == ' ' || input[i + 1] == '\t') i++;
            } else {
                current_arg[arg_pos++] = ch; // add char to current arg
            }
        }

        i++; 
    }

    // Add the last argument if exists
    if (arg_pos > 0) { 
        current_arg[arg_pos] = '\0'; // when the loop ends, null terminate current arg
        argv[argc] = malloc(arg_pos + 1); // allocate memory for the last argument
        if (argv[argc] != NULL) {
            strcpy(argv[argc], current_arg); // copy last arg to argv
            argc++; 
        }
    }

    // Check for unclosed quotes
    if (in_single_quote) {
        fprintf(stderr, "Error: Unclosed single quote\n");
        for (int j = 0; j < argc; j++) free(argv[j]); // free previously allocated memory
        return -1;
    }
    if (in_double_quote) {
        fprintf(stderr, "Error: Unclosed double quote\n");
        for (int j = 0; j < argc; j++) free(argv[j]);
        return -1;
    }

    argv[argc] = NULL; // null terminate argv
    return argc;

/*
- current arg is a temorary buffer on the stack
- it gets overwritten for each new argument
- we need each argument to persists so we copy to heap with malloc
- argv[argc] = NULL: Many functions expect argv to end with NULL
*/
}

// Release allocated memory for argv
void free_argv(char **argv, int argc) {
    for (int i = 0; i < argc; i++) {
        if (argv[i] != NULL) {
            free(argv[i]); //free each arg inside the array of strings
        }
        argv[i] = NULL; // set pointer to NULL after freeing
    }
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
int execute_builtin(char **argv, char *redirect_file, char *redirect_stderr_file, char *append_file, char *append_stderr_file) {
    
    int original_stdout = -1;
    int original_stderr = -1;
    int result = 0;

    // Save original file descriptors BEFORE redirecting
    if (redirect_file || append_file) {
        original_stdout = dup(STDOUT_FILENO);
    }
    if (redirect_stderr_file || append_stderr_file) {
        original_stderr = dup(STDERR_FILENO);
    }

    // Apply redirections using helper
    if (apply_redirection(redirect_file, STDOUT_FILENO, O_WRONLY | O_CREAT | O_TRUNC) == -1) return 1;
    if (apply_redirection(append_file, STDOUT_FILENO, O_WRONLY | O_CREAT | O_APPEND) == -1) return 1;
    if (apply_redirection(redirect_stderr_file, STDERR_FILENO, O_WRONLY | O_CREAT | O_TRUNC) == -1) return 1;
    if (apply_redirection(append_stderr_file, STDERR_FILENO, O_WRONLY | O_CREAT | O_APPEND) == -1) return 1;


    /*
    dup2 does:
    - closes STDOUT_FILENO (fd1)
    - makes STDOUT_FILENO point to the same file as redirect_fd
    - printf() now writes to file and not terminal

    Flags explained:
    - O_WRONLY: Open for writing only
    - O_CREAT: Create file if it doesn't exist
    - O_TRUNC: Truncate (clear) file if it exists
    - O_APPEND: Append to end instead of truncating
    - 0644: Permissions (owner can read/write, others can read)
    */
    
    // === "exit" command ===
    if (strcmp(argv[0], "exit") == 0) {
        save_history_to_histfile();
        if (argv[1] != NULL) {
            // Exit with the provided code
            int code = atoi(argv[1]); // convert string to integer
            exit(code);
        } else {
            // Exit with 0 if no argument provided
            exit(0);
        }
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
            fprintf(stderr, "type: missing argument\n");
            result = 1;
        } else {
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
                    printf("%s: not found\n", argv[1]);
                }
            }
        }
    }
    
    // === "pwd" command ===
    else if (strcmp(argv[0], "pwd") == 0) {
        char *cwd = getcwd(NULL, 0); // system allocates memory for current working directory
        // char cwd[PATH_MAX]; // alternative way with static buffer
        if (cwd != NULL) {
            printf("%s\n", cwd);
            free(cwd); // free memory with the path
        } else {
            perror("getcwd failed");
            result = 1;
        }
    }
    
    // === "cd" command ===
    else if (strcmp(argv[0], "cd") == 0) {
        char *target_dir = argv[1];
        if (target_dir == NULL || strcmp(target_dir,"~") == 0) {
            target_dir = getenv("HOME");
            if (target_dir == NULL) {
                perror("cd: HOME not set\n");
                result = 1;
            }
        }
        if (chdir(target_dir) != 0) {
            fprintf(stderr, "cd: %s: ", target_dir);
	    perror("");
            result = 1;
        }
    }

    
    // === "history" command ===
    else if (strcmp(argv[0], "history") == 0) {
    
        // Handle history -r <filename>
        if (argv[1] != NULL && strcmp(argv[1], "-r") == 0) {
            if (argv[2] == NULL) {
                fprintf(stderr, "history: -r: option requires an argument\n");
                result = 1;
            } else {
                if (read_history(argv[2]) != 0) {
                    fprintf(stderr, "history: %s: ", argv[2]);
                    perror("");
                    result = 1;
                }
            }
        }
        // Handle history -w <filename>
        else if (argv[1] != NULL && strcmp(argv[1], "-w") == 0) {
            if (argv[2] == NULL) {
                fprintf(stderr, "history: -w: option requires an argument\n");
                result = 1;
            } else {
                if (write_history(argv[2]) != 0) {
                    fprintf(stderr, "history: %s: ", argv[2]);
                    perror("");
                    result = 1;
                } else {
                    last_appended_count = history_length;
                }
            }
        }
        // Handle history -a <filename>
        else if (argv[1] != NULL && strcmp(argv[1], "-a") == 0) {
            if (argv[2] == NULL) {
                fprintf(stderr, "history: -a: option requires an argument\n");
                result = 1;
            } else {
                int new_entries = history_length - last_appended_count;
                if (new_entries > 0) {
                    if (append_history(new_entries, argv[2]) != 0) {
                        fprintf(stderr, "history: %s: ", argv[2]);
                        perror("");
                        result = 1;
                    } else {
                        last_appended_count = history_length;
                    }
                }
            }
        }
        else if (argv[1] != NULL) {
            // Show last N entries
            int limit = atoi(argv[1]);
            int start = history_length - limit;
            if (start < 0) start = 0;
            
            for (int i = start; i < history_length; i++) {
                HIST_ENTRY *entry = history_get(i + history_base);
                if (entry != NULL) {
                    printf("%d  %s\n", i + history_base, entry->line);
                }
            }
        }
        else {
            // Show all entries
            HIST_ENTRY **list = history_list();
            if (list != NULL) {
                for (int i = 0; list[i] != NULL; i++) {
                    printf("%d %s\n", i + history_base, list[i]->line);
                }
            }
        }
    }
    // Restore original stdout
    if (original_stdout != -1) {
        dup2(original_stdout, STDOUT_FILENO);
        close(original_stdout);
    }

    // Restore original stderr
    if (original_stderr != -1) {
        dup2(original_stderr, STDERR_FILENO);
        close(original_stderr);
    }
    
    return result;
}


void load_history_from_histfile() {
    char *histfile = getenv("HISTFILE");
    if (histfile != NULL) {
        // Load history from HISTFILE
        read_history(histfile);
        // Remember how many entries we loaded
        initial_history_length = history_length;
        last_appended_count = history_length;
    }
}


void save_history_to_histfile() {
    char *histfile = getenv("HISTFILE");
    if (histfile != NULL) {
        if (initial_history_length > 0) {
            // File had history at startup - APPEND new entries only
            int new_entries = history_length - last_appended_count;
            if (new_entries > 0) {
                append_history(new_entries, histfile);
            }
        } else {
            // File was empty or didn't exist - WRITE all history
            write_history(histfile);
        }
    }
}

// Function to execute external commands
int execute_external(char **argv, char *redirect_file, char *redirect_stderr_file, char *append_file, char *append_stderr_file) {

    char *full_path = find_executable(argv[0]);

    if (full_path == NULL) {
        printf("%s: command not found\n", argv[0]);
        return 1;
    }

    // Create a new process (pid_t = process ID type)
    pid_t pid = fork(); 

    /*
    fork does:
    - creates exact copy of the current process (parent)
    - returns twice:
        - in partent: returns child's PID
        - in child: returns 0
    - both process continue from same point 
    */

    if (pid == -1) {
        perror("fork failed");
        free(full_path);
        return 1;
    }
    else if (pid == 0) {
        // Child process
        
        // 1. Handle STDOUT Redirection (overwrite >)
        if (apply_redirection(redirect_file, STDOUT_FILENO, O_WRONLY | O_CREAT | O_TRUNC) == -1) {
            exit(1);
        }

        // 2. Handle STDOUT Append (append >>)
        if (apply_redirection(append_file, STDOUT_FILENO, O_WRONLY | O_CREAT | O_APPEND) == -1) {
            exit(1);
        }

        // 3. Handle STDERR Redirection (overwrite 2>)
        if (apply_redirection(redirect_stderr_file, STDERR_FILENO, O_WRONLY | O_CREAT | O_TRUNC) == -1) {
            exit(1);
        }

        // 4. Handle STDERR Append (append 2>>)
        if (apply_redirection(append_stderr_file, STDERR_FILENO, O_WRONLY | O_CREAT | O_APPEND) == -1) {
            exit(1);
        }


        extern char **environ; // extern = external variable (used to access environment variables)
        if (execve(full_path, argv, environ) == -1) {
            perror("execve failed");
            exit(1);
        }

        /*
        execve does:
        - Replaces the current process with a new program
        - Never returns on success
        - If it returns, something went wrong

        Flow:
        - Child process created with fork()
        - Child sets up redirections
        - Child calls execve() to become the new program
        - New program runs and eventually exits
        - Parent waits for child to finish
        */
    }
    else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) { // wait and sync with existing child process
            perror("waitpid failed");
        }
        free(full_path);
        
        // Return exit status of the child
        if (WIFEXITED(status)) { // (WIFEXITED = macro to check if child terminated normally)
            return WEXITSTATUS(status); // (WEXITSTATUS = macro to get exit status of child)
        }
        return 1;
    }
    
    return 0;
}

// New multi-stage pipeline function
void execute_pipeline(char ***commands, int num_commands) {
    int pipes[MAX_PIPELINE_CMDS - 1][2];  // Array of pipe file descriptors 
    pid_t pids[MAX_PIPELINE_CMDS];
    
    // Create all needed pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) { // ex. if num_commands = 3, we need 2 pipes: pipe[0] and pipe[1] that connect cmd1->cmd2 and cmd2->cmd3
            perror("pipe failed");
            return;
        }
    }
    
    // Fork and execute each command
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("fork failed");
            return;
        }
        else if (pids[i] == 0) {
            // Child process
            
            // Set up stdin: read from previous pipe (if not first command)
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            
            // Set up stdout: write to next pipe (if not last command)
            if (i < num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Close all pipe file descriptors in child
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command
            if (is_builtin(commands[i][0])) {
                execute_builtin(commands[i], NULL, NULL, NULL, NULL);
                exit(0);
            }
            else {
                char *full_path = find_executable(commands[i][0]);
                if (full_path == NULL) {
                    fprintf(stderr, "%s: command not found\n", commands[i][0]);
                    exit(1);
                }
                extern char **environ;
                execve(full_path, commands[i], environ);
                perror("execve failed");
                free(full_path);
                exit(1);
            }
        }
    }
    
    // Parent: close all pipe file descriptors
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all children
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

int main() {
    // Disable buffering for immediate output
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // initialize readline library
    initialize_readline();
    load_history_from_histfile();

    while (1) {

        char *argv[MAX_ARGS];
        char *redirect_file = NULL;
        char *redirect_stderr_file = NULL;
        char *append_file = NULL;
        char *append_stderr_file = NULL;

        // Read command using readline (tab completion enabled)
        char *input = read_command_readline();

        

        // handle empty input
        if (strlen(input) == 0) {
            free(input);
            continue;
        }

        // Check for pipes and count them
        int pipe_count = 0;
        for (int i = 0; input[i] != '\0'; i++) {
            if (input[i] == '|') {
                pipe_count++;
            }
        }
        // pipe_count tells us how many '|' symbols exist
        // Number of commands = pipe_count + 1

        if (pipe_count > 0) {
            // Handle multi-stage pipeline
            
            char **commands[MAX_PIPELINE_CMDS];  // Array of argv arrays
            char *cmd_strings[MAX_PIPELINE_CMDS];  // Array of command strings before parsing
            int num_commands = pipe_count + 1;  // Number of commands in pipeline
            
            if (num_commands > MAX_PIPELINE_CMDS) {
                fprintf(stderr, "Error: Too many pipeline stages\n");
                free(input);
                continue;
            }
            
            // Split input string by '|' 
            char *saveptr;  //  remember position
            char *token = strtok_r(input, "|", &saveptr); // strtok_r splits input by "|" and returns first token
            
            int cmd_idx = 0;
            while (token != NULL && cmd_idx < num_commands) {
                cmd_strings[cmd_idx] = token;  // Save pointer to this command string
                cmd_idx++;
                token = strtok_r(NULL, "|", &saveptr);  // Get next token
            }

            // Parse each command string into argv
            int parsing_failed = 0;
            
            for (int i = 0; i < num_commands; i++) {
                // Allocate argv array for this command
                commands[i] = malloc(MAX_ARGS * sizeof(char *));
                if (commands[i] == NULL) {
                    perror("malloc failed");
                    parsing_failed = 1;
                    for (int j = 0; j < i; j++) {
                        for (int k = 0; commands[j][k] != NULL; k++) {
                            free(commands[j][k]);
                        }
                        free(commands[j]);
                    }
                    break;
                }
                
                // Parse the command string into arguments
                int argc = parse_command(cmd_strings[i], commands[i]);
                if (argc <= 0) {
                    fprintf(stderr, "Error: Failed to parse command in pipeline\n");
                    parsing_failed = 1;
                    // Clean up what we've allocated so far
                    for (int j = 0; j <= i; j++) {
                        if (commands[j] != NULL) {
                            free(commands[j]);
                        }
                    }
                    break;
                }
            }
 
            if (!parsing_failed) {
                // Execute the pipeline
                execute_pipeline(commands, num_commands);
                
                // Clean up allocated memory 
                for (int i = 0; i < num_commands; i++) {
                    // Free each argument in this command's argv
                    for (int j = 0; commands[i][j] != NULL; j++) {
                        free(commands[i][j]);
                    }
                    // Free the argv array itself
                    free(commands[i]);
                }
            }
            
            free(input);
            continue;  
        }

        // Normal single command execution
        
        // Tokenize command into arguments
        int argc = parse_command(input, argv);
        
        // Handle parsing errors (unclosed quotes)
        if (argc == -1) {
            free(input);
            continue; // Skip to next iteration
        }
        
        // Handle empty command after parsing
        if (argc == 0 || argv[0] == NULL) {
            free(input);
            continue;
        }

        for (int i = 0; i < argc; i++) {

            // stdout redirection
            if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
                if (i + 1 < argc && argv[i + 1] != NULL) {
                    redirect_file = strdup(argv[i + 1]);  // allocate memory for filename (strdup is safer than malloc+strcpy) 
                    // strdup to allocate memory for filename for a safe copy
                    if (!redirect_file) {
                        perror("strdup failed");
                        free_argv(argv, argc);
                        free(input);
                        continue;
                    }
                    // Free the redirection operator and filename from argv
                    free(argv[i]); 
                    free(argv[i + 1]);
                    argv[i] = NULL; // terminate args before redirection
                    argc = i; // update argc to exclude redirection part
                } else {
                    fprintf(stderr, "Error: No file specified for redirection\n");
                    free_argv(argv, argc);
                    free(input);
                    continue; // Skip command execution
                }
                break;
            }

            // stdout appending
            else if (strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "1>>") == 0) {
                if (i + 1 < argc && argv[i+1] != NULL) {
                    append_file = strdup(argv[i + 1]);
                    if (append_file == NULL) {
                        perror("strdup failed");
                        free_argv(argv, argc);
                        free(input);
                        continue;
                    }
                    // free redirect op and filename from argv
                    free(argv[i]);
                    free(argv[i + 1]);
                    argv[i] = NULL; // terminate args before redirection
                    argc = i; // update argc to exclude redirection part
                } else {
                    fprintf(stderr, "Error: No file specified for stdout appending\n");
                    free_argv(argv, argc);
                    free(input);
                    continue;
                }
                break;
            }

            // stderr redirection
            else if (strcmp(argv[i], "2>") == 0) {
                if (i + 1 < argc && argv[i + 1] != NULL) {
                    redirect_stderr_file = strdup(argv[i + 1]); 
                    if (redirect_stderr_file == NULL) {
                        perror("strdup failed");
                        free_argv(argv, argc);
                        if (redirect_file != NULL) free(redirect_file);
                        free(input);
                        continue;
                    }
                    // Free the redirection operator and filename from argv
                    free(argv[i]); 
                    free(argv[i + 1]);
                    argv[i] = NULL; 
                    argc = i; // update argc to exclude redirection part
                } else {
                    fprintf(stderr, "Error: No file specified for stderr redirection\n");
                    free_argv(argv, argc);
                    if (redirect_file) free(redirect_file);
                    free(input);
                    continue;
                }
                break;
            }      
            else if (strcmp(argv[i], "2>>") == 0) {
                if (i + 1 < argc && argv[i + 1] != NULL) {
                    append_stderr_file = strdup(argv[i + 1]);
                    if (!append_stderr_file) {
                        perror("strdup failed");
                        free_argv(argv, argc);
                        if (redirect_file) free(redirect_file);
                        free(input);
                        continue;
                    }
                    free(argv[i]);
                    free(argv[i + 1]);
                    argv[i] = NULL;
                    argc = i;
                } else {
                    fprintf(stderr, "Error: No file specified for stderr append\n");
                    free_argv(argv, argc);
                    if (redirect_file) free(redirect_file);
                    free(input);
                    continue;
                }
                break;
            }
        }
    
        // Execute command
        if (is_builtin(argv[0])) {
            execute_builtin(argv, redirect_file, redirect_stderr_file, append_file, append_stderr_file);
        } else {
            execute_external(argv, redirect_file, redirect_stderr_file, append_file, append_stderr_file);
        }
        
        // Free allocated memory for arguments
        free_argv(argv, argc);
        
        // Free redirect files if allocated
        if (redirect_file != NULL) {
            free(redirect_file);
        }
        if (redirect_stderr_file != NULL) {
            free(redirect_stderr_file);
        }
        if (append_file != NULL) {
            free(append_file);
        }
        if (append_stderr_file != NULL) {
            free(append_stderr_file);
        }
        
        free(input);
    }

    return 0;
}