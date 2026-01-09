# Study: Building a Shell in C

## Theory

A shell is a command-line interface that executes commands and manages processes. This project implements a **POSIX-compliant shell** capable of interpreting shell commands, executing external programs, and running builtin commands.

**Learning Goals:**

* Deepen understanding of **C programming** and low-level system interactions.
* Explore **process management**, **memory handling**, and **pointer usage**.
* Master **file descriptors** and **I/O redirection**.
* Understand the mechanics behind **Inter-Process Communication (IPC)** via pipes.


## Features Implemented

### 1. Prompt and Input Handling
* **GNU Readline Integration:** Replaced standard `fgets` with `readline` to support line editing (arrows, delete) and history navigation (up/down).
* **Dynamic Prompt:** Generates a colored prompt displaying `[Time] User@Hostname:path$`.

### 2. Command Parsing
* **Tokenization:** Splits input strings into arguments while respecting **single (`'`) and double (`"`) quotes**.
* **Escape Characters:** Handles backslash escapes inside double quotes (e.g., `\"` or `\\`).
* **Memory Management:** Uses dynamic allocation for argument arrays (`argv`) to prevent buffer overflows.

### 3. Builtin Commands
* `exit` – Exits the shell (supports optional exit codes).
* `echo` – Prints arguments to stdout.
* `type` – Identifies whether a command is a builtin or an external executable.
* `pwd` – Prints the current working directory.
* `cd` – Changes the current directory (supports `~` expansion and relative paths).
* `history` – **Persistent history management**.
    * `-r` (read), `-w` (write), and `-a` (append) specific files.
    * Automatically loads/saves history to `HISTFILE` on startup/exit.

### 4. External Command Execution
* **Path Resolution:** Manually parses the `PATH` environment variable to locate executables.
* **Process Creation:** Uses `fork()` to create child processes.
* **Execution:** Uses `execve()` to replace the child process image with the target program.
* **Synchronization:** Parent process uses `waitpid()` to wait for child termination.

### 5. I/O Redirection
* **Mechanism:** Uses `open()`, `close()`, and `dup2()` to manipulate standard streams.
* **Modes Supported:**
    * `>` : Overwrite stdout
    * `>>` : Append stdout
    * `2>` : Overwrite stderr
    * `2>>` : Append stderr

### 6. Pipelines
* **Multi-stage Pipes:** Supports chaining commands via `|` (e.g., `ls | grep .c | wc -l`).
* **IPC:** Uses `pipe()` to create unidirectional data channels between sibling processes.
* **FD Management:** Carefully closes unused pipe ends in parent and children to prevent hangs/deadlocks.

### 7. Tab Completion
* **Context Aware:** Hooks into `rl_attempted_completion_function`.
* **Scope:** Scans both **builtin commands** and **executables in PATH**.
* **Functionality:** Hitting `TAB` auto-completes the command name.



## Key Technical Concepts Learned

1.  **The Process Lifecycle:**
    * Understanding how `fork()` duplicates the memory space and how `execve()` replaces it.
    * Handling "zombie" processes using `waitpid` macros (`WIFEXITED`, `WEXITSTATUS`).

2.  **File Descriptors & Streams:**
    * "Everything is a file": How `stdin` (0), `stdout` (1), and `stderr` (2) can be manipulated using `dup2` to redirect output to files or pipes.

3.  **Memory Safety:**
    * Manual management of the heap (`malloc`, `free`, `strdup`).
    * Preventing memory leaks by rigorously freeing argument vectors (`argv`) after execution.

4.  **String Parsing:**
    * Low-level string manipulation without using high-level regex libraries (handling quotes and whitespace manually).


## Compilation and Execution

**Dependencies:**
Ensure you have the `readline` library installed (e.g., `libreadline-dev` on Debian/Ubuntu).

```bash
# Compile the shell
gcc main.c -o shell -lreadline

# Run
./shell
```


## Possible Future Improvements (Modern UX & Productivity)

To evolve this shell into a **"Comfy" daily driver** (inspired by Fish and Zsh), the focus shifts to **usability**, **visual feedback**, and **workflow efficiency**.

### 1. Enhanced User Experience (UX)

* **Smart Tab Completion:**
* Expand completion to include **file paths** and directories (currently only supports commands).
* Context-aware completion (e.g., `git <TAB>` suggests git subcommands).


* **Syntax Highlighting:**
* Real-time coloring of the command line.
* **Valid commands** in Green, **Invalid** in Red, **Strings** in Yellow.


* **Autosuggestions:**
* "Ghost text" suggestions based on command history (similar to Fish shell).



### 2. Job Control & Multitasking

* **Background Processes:** Support `&` to run commands without blocking the shell.
* **Process Management:** Implement `Ctrl+Z` (suspend), `fg` (foreground), and `bg` (background).
* **Signal Handling:** Proper propagation of `SIGINT` (Ctrl+C) to child processes instead of killing the shell itself.

### 3. Customization & Scripting

* **Configuration File:** Load a `~/.myshellrc` file on startup to define persistent preferences.
* **Aliases:** Implement `alias` command (e.g., `alias ll="ls -la"`) for user shortcuts.
* **Variables:** Support setting (`VAR=value`) and expanding (`$VAR`) local shell variables.
* **Control Flow:** Support `&&` (AND) and `||` (OR) operators for conditional execution.

