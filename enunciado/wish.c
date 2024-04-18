#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // For open()
#include <ctype.h> // For isspace()
#include <stdbool.h> // to use bool type


#define MAX_LINE 1024 // Max input line size

// Global debug mode flag
int debug_mode = 0;

////////#########////////  FUNCTION PROTOTYPES  ////////#########////////


void execute_external_command(char **args);
void handle_redirection(char **args, int *arg_count);
void execute_commands_in_parallel(char **commands, int num_commands);
void parse_command_to_args(char *command, char **args);


////////#########////////  END FUNCTION PROTOTYPES  ////////#########////////





////////#########////////  HELPER FUNCTIONS  ////////#########////////



// helper funct to handle arg counting
int count_args(char **args) {
    int count = 0;
    while (args[count] != NULL) count++;
    return count;
}

// helper funct to parse command to args
void parse_command_to_args(char *command, char **args) {
    int i = 0;
    char *token;
    while ((token = strsep(&command, " \t\n")) != NULL) {
        if (strlen(token) > 0) {
            args[i++] = token;
        }
    }
    args[i] = NULL;
}


int is_line_empty_or_whitespace(const char* line) {
    while (*line != '\0') {
        if (!isspace((unsigned char)*line))
            return 0; // Found a non-whitespace character
        line++;
    }
    return 1; // Line is empty or all whitespace
}


void split_commands(char* line, char*** commands, int* num_commands) {
    int capacity = 10; // Initial capacity
    *commands = malloc(capacity * sizeof(char*)); // Dynamically allocated array of command strings
    if (*commands == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    *num_commands = 0;
    char* token = strtok(line, "&");
    while (token != NULL) {
        if (*num_commands >= capacity) {
            // Increase capacity
            capacity *= 2;
            *commands = realloc(*commands, capacity * sizeof(char*));
            if (*commands == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }
        }
        
        (*commands)[*num_commands] = strdup(token); // Duplicate and store the command
        if ((*commands)[*num_commands] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }
        (*num_commands)++;
        token = strtok(NULL, "&");
    }
}



////////#########//////// END HELPER FUNCTIONS  ////////#########////////






////////#########//////// PATH HANDLING SHIT ////////#########//////// 


// struct definition for global PATH
typedef struct {
    char **paths; // Array of strings
    int count;    // Number of paths
    int capacity; // Current capacity of the array
} PathList;


// global struct to store PATH
PathList globalPathList;



// Initialize the path list with a default size
void initPathList(PathList *pathList, int capacity) {
    pathList->paths = (char**)malloc(capacity * sizeof(char*));
    // Check if malloc succeeded
    if (pathList->paths == NULL) {
        // Handle memory allocation failure
        fprintf(stderr, "Error: Memory allocation failed in initPathList\n");
        exit(EXIT_FAILURE); // Exit the program with a failure status
    }
    pathList->count = 0;
    pathList->capacity = capacity;
}




// Free the memory used by the path list
void freePathList(PathList *pathList) {
    for (int i = 0; i < pathList->count; ++i) {
        free(pathList->paths[i]); // Free each path string
    }
    free(pathList->paths); // Free the array of pointers
}



// Clears the current paths and reallocates the paths array
void clearPaths(PathList *pathList) {
    freePathList(pathList); // Free current paths
    initPathList(pathList, pathList->capacity); // Reinitialize
}



// Add a path to the list, expanding the array if necessary dynamicaly
void addPath(PathList *pathList, const char *path) {
    if (pathList->count == pathList->capacity) {
        // Need more space
        int newCapacity = pathList->capacity * 2;
        char **newPaths = (char**)realloc(pathList->paths, newCapacity * sizeof(char*));
        if (!newPaths) {
            perror("Unable to expand path list");
            return;
        }
        pathList->paths = newPaths;
        pathList->capacity = newCapacity;
    }
    // Allocate space for the new path and copy it
    pathList->paths[pathList->count] = strdup(path);
    pathList->count++;
}



// Initializes the default path. Its called always when the program starts.
// For more default stuff, put it here using addPath like is used below.
void initDefaultPath(PathList *pathList) {
    addPath(pathList, "/bin"); // Add "/bin" as the default search path
}



// print the current paths at PATH
void printCurrentPaths() {
    printf("Current paths:\n");
    if (globalPathList.count == 0) {
        printf("  (empty)\n");
    }
    for (int i = 0; i < globalPathList.count; i++) {
        printf("  %s\n", globalPathList.paths[i]);
    }
}


////////#########////////  END PATH HANDLING SHIT ////////#########//////// 



////////#########////////  SHELL EXECUTION LOGIC ////////#########//////// 


// Checks if a given command is builtin or not
// returns 1 if built-in; if not buil-in returns 0
int check_builtin_commands(char **args, int arg_count) {
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    } else if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || args[2] != NULL) {
            fprintf(stderr, "wish: cd requires exactly one argument\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("wish: cd");
            }
        }
        return 1; // code indicating is a built in command
    } 
    if (strcmp(args[0], "path") == 0) {
        clearPaths(&globalPathList); // Clear existing paths
        for (int i = 1; i < arg_count; i++) {
            addPath(&globalPathList, args[i]); // Add new paths
        }

        printCurrentPaths(); // Print the current paths after updating
        return 1; // Indicating it's a built-in command
    }

    return 0; // Not a built-in command
}

// Parses commands and executes them
void process_command(char *line) {
    if (debug_mode) printf("Log: Starting process_command() with line: %s\n", line);

    // Trim the line and check if it's empty or all whitespace
    if (is_line_empty_or_whitespace(line)){
        if (debug_mode) printf("Debug: Command is empty or whitespace\n");

        return;
    }

    char** commands;
    int num_commands = 0;
    split_commands(line, &commands, &num_commands); // Split line into parallel commands
    if (debug_mode) printf("Debug: Number of commands to process: %d\n", num_commands);

    if (num_commands == 1) {
        if (debug_mode) printf("Log: Processing a single command in process_command()\n");

        // If only one command, process normally.
        char *args[MAX_LINE / 2 + 1]; // Command arguments
        parse_command_to_args(commands[0], args); // Parse command string to args
        
        if (!check_builtin_commands(args, count_args(args))) {
            execute_external_command(args); // Execute if not a built-in command
        }
    } else {
        if (debug_mode) printf("Debug: Multiple commands detected, executing in parallel\n");
        
        // If multiple commands, execute them in parallel.
        execute_commands_in_parallel(commands, num_commands);
    }

    // Free dynamically allocated commands array after execution
    for (int i = 0; i < num_commands; i++) {
        free(commands[i]);
    }
    free(commands);
    if (debug_mode) printf("Debug: Finished processing command\n");
    
    // In interactive mode, the prompt display is handled by the caller (main loop)
}




////////#########//////// END SHELL EXECUTION LOGIC ////////#########////////




////////#########//////// EXTERNAL EXECUTION LOGIC ////////#########////////


/**            ####     char* findExecutable(char* command)    ####
 * 
 * Attempts to find the full path to an executable command.
 * 
 * This function searches for an executable file by the name specified in the `command` parameter.
 * If the command includes a slash (`/`), it is assumed to be either an absolute or a relative path,
 * and the function checks the specified path for executability. If no slash is present, the function
 * iterates through the directories listed in the global `PathList` (globalPathList), constructing
 * paths by combining each directory with the command name, and checks each for executability.
 * 
 * The search respects the order of directories in `globalPathList`, returning the path to the
 * first executable match found.
 * 
 * Parameters:
 *   command - A string representing the command to find. This can be just the name of the command
 *             (for example "ls") or a path to the command (either absolute like "/bin/ls" or relative like
 *             "./give_permissions_to_all_scripts.sh").
 * 
 * Returns:
 *   A dynamically allocated string containing the full path to the executable command if found.
 *   The caller is responsible for freeing this string using `free`.
 *   If the command is not found or is not executable, returns NULL.
 * 
 * Note:
 *   The function uses `access()` with the `X_OK` flag to check for the command's executability.
 *   If `command` contains a slash, it's directly checked without searching `globalPathList`.
 */
char* findExecutable(char* command) {
    if (strchr(command, '/') != NULL) {
        // Command already contains a path (absolute or relative)
        if (access(command, X_OK) == 0) {
            return strdup(command); // Return the command as is
        }
    } else {
        // Search for the command in the specified paths
        for (int i = 0; i < globalPathList.count; i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", globalPathList.paths[i], command);
            if (access(path, X_OK) == 0) {
                return strdup(path); // Return the full path to the executable
            }
        }
    }
    return NULL; // Command not found
}



/**       
 * Executes an external command with optional output redirection.
 * 
 * This function is responsible for executing a command specified by the `args` array, 
 * handling output redirection if a '>' operator is present among the arguments. The command 
 * to execute is determined by `args[0]`, and any arguments to the command follow in the array. 
 * If redirection is specified (indicated by '>'), the output of the command is written to the 
 * file named immediately after this operator, creating or truncating the file as necessary.
 * 
 * The function first searches for the executable in the filesystem using `findExecutable`, 
 * which checks both directly specified paths (e.g., "./script.sh" or "/bin/ls") and searches 
 * the directories listed in the global path list for the executable. It then forks a new process 
 * to execute the command. In the child process, if redirection is specified, it adjusts the 
 * standard output to the target file before executing the command using `execv`. The parent 
 * process waits for the child process to complete before returning.
 * 
 * Detailed logging statements are included to facilitate debugging and tracing the execution 
 * flow, including indicating when redirection is handled, when the child process is forked, 
 * and when execution of the external command is attempted.
 * 
 * @param args An array of strings representing the command and its arguments. The last element 
 *             must be NULL. If redirection is specified, the array will include a '>' followed 
 *             by a filename, and the array will be terminated before the '>' for execv execution.
 */
void execute_external_command(char **args) {
    if (debug_mode) printf("Log: Starting execute_external_command()\n");
    
    int redirect_index = -1;
    int fd = -1; // File descriptor for redirection, if needed

    // Search for redirection symbol ('>') and note its index
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            redirect_index = i;
            if (debug_mode) printf("Log: Redirection found at index %d\n", redirect_index);
            break;
        }
    }

    char* executablePath = findExecutable(args[0]);
    if (!executablePath) {
        fprintf(stderr, "wish: command not found: %s\n", args[0]);
        return;
    }

    pid_t pid = fork();

    if (pid == 0) { // Child process
        if (debug_mode) {
            printf("Log: Forked in execute_external_command() with pid = 0\n");
            printf("Log: In child process (execute_external_command)\n");
        }
        // Handle redirection by replacing STDOUT
        if (redirect_index != -1) {
            fd = open(args[redirect_index + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("wish: open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[redirect_index] = NULL; // Terminate args before '>'
        }

        // Execute the command
        if (debug_mode) printf("Log: Executing %s\n", executablePath);
        if (execv(executablePath, args) == -1) {
            perror("wish: execv");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        if (debug_mode) printf("Log: Forked in execute_external_command() with pid = %d\n", pid);
        // Parent process waits for the child process to complete
        int status;
        waitpid(pid, &status, 0);
        if (debug_mode) printf("Log: Child process completed\n");
    } else {
        perror("wish: fork");
    }
    
    free(executablePath); // Free dynamically allocated path
    if (debug_mode) printf("Log: Finished execute_external_command()\n");
}




void execute_commands_in_parallel(char **commands, int num_commands) {
    if (debug_mode) printf("Log: Starting execute_commands_in_parallel() with %d commands\n", num_commands);
    
    pid_t pids[num_commands]; // Array to store child PIDs

    for (int i = 0; i < num_commands; i++) {
        if (debug_mode) printf("Log: Forking command %d in execute_commands_in_parallel()\n", i);

        pids[i] = fork();
        
        if (pids[i] == 0) { // Child process
            char *args[MAX_LINE / 2 + 1]; // Array for command arguments
            parse_command_to_args(commands[i], args); // Parse command string to args
            
            if (!check_builtin_commands(args, count_args(args))) {
                execute_external_command(args); // Execute if not a built-in command
            }
            exit(0); // Exit after execution
        } else if (pids[i] < 0) {
            perror("wish: fork");
            exit(EXIT_FAILURE); // Forking failed
        }
    }

    // Parent waits for all child processes
    for (int i = 0; i < num_commands; i++) {
        if (debug_mode) printf("Log: Waiting for command %d to finish in execute_commands_in_parallel()\n", i);

        waitpid(pids[i], NULL, 0);
    }
    if (debug_mode) printf("Log: Ending execute_commands_in_parallel()\n");

}




////////#########//////// END EXTERNAL EXECUTION LOGIC ////////#########////////






int main(int argc, char *argv[]) {

    // Process command-line arguments to set debug_mode
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: wish [options]\n");
            printf("Options:\n");
            printf("  --help        Display this help message and exit\n");
            printf("  --debug       Run in debug mode\n");
            return 0;  // Exit after displaying help
        }
    }

    initPathList(&globalPathList, 10);
    initDefaultPath(&globalPathList);

    char *line = NULL;
    size_t linecap = 0;
    FILE *input_stream = stdin;
    // Check if the standard input is a terminal
    bool isInteractive = isatty(STDIN_FILENO);

    if (debug_mode) {
        printf("Debug: Starting shell in %s mode\n", isInteractive ? "interactive" : "batch");
    }

    // Display initial prompt if in interactive mode
    if (isInteractive) {
        printf("wish> ");
        fflush(stdout); // Ensure the prompt is displayed immediately
    }

    while (getline(&line, &linecap, input_stream) != -1) {
        process_command(line);

        // Display prompt after command execution in interactive mode
        if (isInteractive) {
            if (debug_mode) printf("Debug: Command processing finished, displaying prompt\n");
            printf("wish> ");
            fflush(stdout); // Ensure the prompt is displayed immediately after printing
        }
    }

    if (feof(stdin)) {
        printf("\n"); // Print a newline if we reach the end of input (EOF)
    }

    if (input_stream != stdin) {
        fclose(input_stream); // Close the batch file if we're in batch mode
    }

    free(line);
    freePathList(&globalPathList);
    if (debug_mode) printf("Debug: Exiting shell\n");

    return 0;
}
