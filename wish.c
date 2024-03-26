#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE 1024 // Max input line size



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

// parses commands and executes them
void process_command(char *line) {
    char *args[MAX_LINE / 2 + 1]; // Command arguments
    char *token;
    int arg_count = 0;

    // Parse the input line into arguments
    while ((token = strsep(&line, " \t\n")) != NULL) {
        if (strlen(token) > 0) {
            args[arg_count++] = token;
        }
    }
    args[arg_count] = NULL; // Null-terminate the array

    if (arg_count == 0) { // Empty command
        return;
    }

    if (!check_builtin_commands(args, arg_count)) {
        execute_external_command(args); 
    }
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
 *             (e.g., "ls") or a path to the command (either absolute like "/bin/ls" or relative like
 *             "./myscript").
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



// executes an external command
void execute_external_command(char **args) {
    char* executablePath = findExecutable(args[0]);
    if (!executablePath) {
        fprintf(stderr, "wish: command not found: %s\n", args[0]);
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        if (execv(executablePath, args) == -1) {
            perror("wish: execv");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to finish
    } else {
        // Error forking
        perror("wish: fork");
    }
    
    free(executablePath); // Free the dynamically allocated path
}



////////#########//////// END EXTERNAL EXECUTION LOGIC ////////#########////////






int main(int argc, char *argv[]) {
    initPathList(&globalPathList, 10);
    initDefaultPath(&globalPathList);

    char *line = NULL; // Stores the input line
    size_t linecap = 0; // Capacity of the line
    FILE *input_stream = stdin; // Default to standard input

    // Check for batch mode or interactive mode
    if (argc == 1) {
        // Interactive mode
        printf("wish> ");
    } else if (argc == 2) {
        // Batch mode
        input_stream = fopen(argv[1], "r");
        if (!input_stream) {
            fprintf(stderr, "wish: error opening file %s\n", argv[1]);
            exit(1);
        }
    } else {
        fprintf(stderr, "Usage: ./wish or ./wish <batch file>\n");
        exit(1);
    }

    // Main loop for reading and processing commands
    while (getline(&line, &linecap, input_stream) != -1) {
        process_command(line);
        if (input_stream == stdin) printf("wish> "); // Only print prompt in interactive mode
    }

    if (feof(stdin)) {
        printf("\n"); // Ensure we start on a new line after EOF
    }

    if (input_stream != stdin) fclose(input_stream); // Close the file if in batch mode
    free(line); // Free the allocated line buffer

    freePathList(&globalPathList);
    return 0;
}
