# A Custom Unix Shell Implementation

A feature-rich Unix shell implementation written in C, demonstrating advanced systems programming concepts including process management, signal handling, I/O redirection, and inter-process communication through pipes.

## Features

### Built-in Commands
- **`cd [directory]`** - Change working directory (defaults to HOME if no argument)
- **`exit`** - Terminate the shell and cleanup all background processes
- **`estatus`** - Display exit status of the most recently completed command
- **`bglist`** - List all currently running background jobs
- **`fg [pid]`** - Bring a background job to the foreground

### Process Management
- **Background Execution** - Run commands asynchronously with `&`
- **Job Tracking** - Automatic monitoring and reaping of background processes
- **Process Limit** - Optional maximum background process limit
- **Exit Status Tracking** - Captures and stores exit codes from completed processes

### Signal Handling
- **SIGCHLD** - Automatic detection and cleanup of terminated child processes
- **SIGUSR2** - Custom signal handler with ANSI color-coded output
- **Async-Signal-Safe** - All signal handlers use only reentrant functions

### I/O Redirection
- **Input Redirection** (`<`) - Read from file instead of stdin
- **Output Redirection** (`>`) - Write to file instead of stdout
- **Error Redirection** (`2>`) - Redirect stderr to file
- **Validation** - Prevents invalid file combinations

### Pipes
- **Single Pipe** (`cmd1 | cmd2`) - Connect output of one command to input of another
- **Multiple Pipes** (`cmd1 | cmd2 | cmd3`) - Chain up to 3 processes together
- **Proper FD Management** - Ensures all file descriptors are correctly managed and closed

## Technical Implementation

### Core Technologies
- **Language:** C (C11 standard)
- **System Calls:** fork, exec, wait, pipe, dup2, open, close, signal
- **Libraries:** readline (for input), standard POSIX libraries

### Architecture Highlights

#### Process Creation & Execution
- Uses `fork()` to create child processes
- Employs `execvp()` for command execution with PATH resolution
- Implements proper parent-child synchronization with `waitpid()`

#### Memory Management
- Linked list data structure for background job tracking
- Proper cleanup and deallocation of all dynamically allocated memory
- Zero memory leaks (Valgrind clean)

#### Signal Safety
- All signal handlers use only async-signal-safe functions
- Utilizes `volatile sig_atomic_t` for shared variables
- Flag-based approach for deferred signal processing

#### File Descriptor Management
- Comprehensive tracking and closing of all file descriptors
- Proper inheritance and redirection setup
- Prevents deadlocks and hanging processes in pipe implementations

## Building

```bash
# Compile in release mode
make

# Compile with debugging symbols
make debug

# Clean build artifacts
make clean
```

## Usage

### Starting the Shell
```bash
./bin/53shell [max_bg_processes]
```

**Optional argument:**
- `max_bg_processes` - Maximum number of concurrent background processes (default: unlimited)

### Example Commands

#### Basic Execution
```bash
<53shell>$ ls -la
<53shell>$ pwd
<53shell>$ echo "Hello, World!"
```

#### Background Processes
```bash
<53shell>$ sleep 10 &
<53shell>$ bglist
[1] 12345 Mon Nov 4 10:30:15 2024 sleep 10 &
<53shell>$ fg 12345
sleep 10 &
```

#### I/O Redirection
```bash
<53shell>$ ls > files.txt
<53shell>$ cat < input.txt
<53shell>$ grep pattern file.txt 2> errors.txt
```

#### Pipes
```bash
<53shell>$ ls -l | grep ".c"
<53shell>$ cat file.txt | grep "pattern" | wc -l
<53shell>$ echo "data" | sort | uniq
```

#### Directory Navigation
```bash
<53shell>$ cd /tmp
/tmp
<53shell>$ cd
/home/user
<53shell>$ cd ..
/home
```

#### Exit Status
```bash
<53shell>$ ls
file1.txt  file2.txt
<53shell>$ estatus
0
<53shell>$ cat nonexistent
cat: nonexistent: No such file or directory
<53shell>$ estatus
1
```

### Signal Testing
```bash
# In one terminal
<53shell>$ echo $$
12345

# In another terminal
$ kill -SIGUSR2 12345

# First terminal will display
Hi username!
```

## Project Structure

```
.
├── src/
│   ├── icssh.c          # Main shell implementation
│   ├── helpers.c        # Helper functions for job management
│   └── linkedlist.c     # Linked list implementation
├── include/
│   ├── icssh.h          # Main header file
│   ├── helpers.h        # Helper function declarations
│   ├── linkedlist.h     # Linked list interface
│   └── debug.h          # Debug macros and utilities
├── lib/
|   └── icsshlib.o       # Object file
└── Makefile            # Build configuration
```

## Implementation Details

### Background Job Management
- Jobs stored in a time-sorted linked list (oldest to newest)
- Automatic cleanup via SIGCHLD signal handler
- Non-blocking reaping with `WNOHANG` flag
- Proper memory management for job metadata

### Pipe Implementation
- Creates `(N-1)` pipes for `N` processes
- Each child process connects to appropriate pipe ends
- All unused file descriptors are closed to prevent hangs
- Parent waits for all children and uses last process's exit status

### I/O Redirection
- Opens files with appropriate flags (`O_RDONLY`, `O_WRONLY | O_CREAT | O_TRUNC`)
- Uses `dup2()` to redirect file descriptors
- Validates against invalid file combinations
- Proper error handling for file operations

### Signal Handlers
- **SIGCHLD**: Sets a flag for deferred processing in main loop
- **SIGUSR2**: Manually constructs colored message using only `write()`
- Pre-caches username to avoid calling `getenv()` in handler

## Error Handling

The shell provides clear error messages for common issues:
- `REDIRECTION ERROR` - Invalid file combinations or file access errors
- `DIRECTORY ERROR` - Directory does not exist or cannot be accessed
- `EXEC ERROR` - Cannot execute specified command
- `WAIT ERROR` - Error while waiting for process
- `PROCESS ERROR` - Invalid process ID
- `PIPE ERROR` - Invalid use of pipe operators
- `BG ERROR` - Maximum background processes exceeded

## Future Enhancements

Potential improvements for future versions:
- [ ] Append mode redirection (`>>`)
- [ ] Combined stdout/stderr redirection (`&>`)
- [ ] More robust parser for quoted strings and escape sequences
- [ ] Job control with `SIGTSTP` and `SIGCONT`
- [ ] Command history and tab completion
- [ ] Environment variable expansion
- [ ] Wildcard pattern matching

## System Requirements

- **OS:** Linux (tested on Ubuntu 24)
- **Compiler:** GCC with C11 support
- **Libraries:** readline-dev
- **Make:** GNU Make

### Installing Dependencies (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential libreadline-dev
```

## Testing

The shell has been tested with:
- Memory leak detection (Valgrind)
- Multiple concurrent background processes
- Complex pipe chains
- Various redirection combinations
- Signal handling under load
- Edge cases and error conditions

## Acknowledgments

Built with a focus on proper systems programming practices, including:
- Defensive programming and error checking
- Resource management and cleanup
- Signal safety and reentrant code
- POSIX compliance
