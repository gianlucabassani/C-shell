# Study: Building a Shell in C

## Theory

A shell is a command-line interface that executes commands and manages processes.
This project implements a **POSIX-compliant shell** capable of interpreting shell commands, executing external programs, and running builtin commands.

**Learning goals:**

* Deepen understanding of **C programming** and low-level system interactions.
* Explore **process management**, **memory handling**, and **pointer usage**.
* Setting up for extending the shell into **penetration testing / offensive security tools**.

---

## Features Implemented

### 1. Prompt and Input Handling

* Basic REPL loop (`$ ` prompt) using `fgets` for reading commands.

### 2. Command Parsing

* Tokenizes input into command and arguments (`argv` array).
* Supports up to 64 arguments per command.

### 3. Builtin Commands

* `exit` – exits the shell.
* `echo` – prints arguments to stdout.
* `type` – identifies whether a command is builtin or external.
* `pwd` – prints current working directory (`getcwd`, dynamic allocation).
* `cd` – changes current directory, supports `$HOME` and relative paths.

### 4. External Command Execution

* Finds executables in `PATH`.
* Uses `fork()` to create child processes.
* Uses `execve()` to execute commands.

### 5. I/O Redirection

* Stdout redirection: > (saving to file) and >> (appending to file).
* Stderr redirection: 2> (saving to file) and 2>> (appending to file).
* Works with both builtin and external commands.

### 6. Tab Completion (IN PROGRESS)

* Basic command completion for builtin commands using GNU readline library.
* Completes ech → echo  and exi → exit  with trailing space.
* Only completes at command position (not arguments).

---

## Compilation and Execution

```bash
gcc  main.c -o shell -lreadline

./shell
```

---

## Possible Future Improvements (Cybersecurity / Offensive Security)

1. **Command obfuscation and encoding**

   * Encode/decode commands to avoid detection.
   * Support Base64 or XOR encoding for input/output.

2. **Enhanced file system operations**

   * Commands like `ls`, `cat`, `download`, `upload`.
   * Recursive file search and enumeration of sensitive directories.

3. **Process management**

   * Spawn and manage background processes.
   * Inject scripts or commands into running processes.

4. **Networking capabilities**

   * Reverse shells over TCP/UDP.
   * Bind shells and port forwarding.

5. **In-memory execution**

   * Execute binaries directly in memory (Meterpreter style).
   * Shellcode execution and dynamic library loading.

6. **Persistence and stealth**

   * Hidden shell sessions.
   * Redirection of stdout/stderr to avoid logs.
