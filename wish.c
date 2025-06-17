/*
Name: Juuso von Behr
Student number: 0617328
Date: 14.6.2025
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

//Define elimiters and a buffer
#define DELIMS " \t\r\n\a"
#define BUFSIZE 64

//Declaring all methods
void shellLoop(int mode, FILE *input);
char **split_line(char *line, int *commandAmount);
char **parse_commands(char *line);
int redirect(char **args, char **fileName);
char *search_executable_path(char *command);
int execute_command(char **args);
void error_message();

//Global variables, tried having these as variables in main at first, but since they're needed in so many methods, decided to make them global
char *search_paths[100];
int path_amount = 1;

// Got help for a lot of the basic functionalities from here: https://brennan.io/2015/01/16/write-a-shell-in-c/
int main(int argc, char *argv[]){
    FILE *input = stdin;
    search_paths[0] = strdup("/bin");
    
    // Empty the array, except the /bin
    for(int i=1; i<100; i++){
        search_paths[i] = NULL;
    }

    //If 1 argument take input from user, if 2 arguments take input from file, otherwise error
    if(argc == 1){
        input = stdin;
        shellLoop(0, input);

    } else if(argc == 2){
        input = fopen(argv[1], "r");
        if(input == NULL){
            error_message();
            exit(1);
        }
        shellLoop(1, input);

    } else{
        error_message();
        exit(1);
    }

    // Free memory from paths
    for (int i = 0; i < 100; i++) {
        if (search_paths[i] != NULL) {
            free(search_paths[i]);
        }
    }

    return 0;
}

// The method inside of which the while loop for program execution is.
void shellLoop(int mode, FILE *input){
    char *line = NULL;
    int status = 1, commandAmount = 0;
    size_t linesize = 0;
    ssize_t chars;

    //Run while status is 1, end otherwise (when zero)
    do{
        // Mode 0 user input and mode 1 file input
        if(mode == 0){
            printf("wish> ");
        }
     
        chars = getline(&line, &linesize, input);
        
        //Check if EOF is reached
        if (chars == -1){
            if(mode == 1){
                printf("\n");
            }
            status = 0;
            break;
        }

        //Line is split into commands
        char **commands = split_line(line, &commandAmount);

        if(commandAmount == 0){
            free(commands);
            continue;
        }

        pid_t children[100];
        int childAmount = 0;

        //Loop for parsing the commands and executing them
        for(int i=0; i<commandAmount; i++){
            char **args = parse_commands(commands[i]);
            
            if(args[0] != NULL){
                int result = execute_command(args);
                if(result > 1){
                    children[childAmount] = result;
                    childAmount++;
                }

                //Free args array, is this necessary?
                for(int j=0; args[j] != NULL; j++){
                    free(args[j]);
                }
            }
            free(args);
        }

        // Loop for parallel execution
        for(int i=0; i<childAmount; i++){
            int state;
            waitpid(children[i], &state, 0);
        }

        //Free command array memory and set command amounts to zero
        for(int i=0; i<commandAmount; i++){
            free(commands[i]);
        }
        free(commands);
        commandAmount = 0;

    } while(status == 1);

    //Free memory of lines
    if(line){
        free(line);
    }

    return;
}

//Method for splitting a line into commands
char **split_line(char *line, int *commandAmount){
    int buffer = BUFSIZE, pos = 0;
    char **commands = malloc(buffer * sizeof(char*));
    char *token;
    char *line_copy = strdup(line); // The line is copied because strtok modifies the original string, so using line itself didn't work

    if(!commands || !line_copy){
        error_message();
        exit(1);
    }

    //Tokenize with &
    token = strtok(line_copy, "&");
    while(token != NULL){
        // Below while trims whitespace
        while(*token == ' '|| *token == '\t' || *token == '\n'){
            token++;
        }

        if(strlen(token) > 0){
            commands[pos] = strdup(token); // strdup to ease memory management
            pos++;

            if(pos >= buffer){
                buffer += BUFSIZE;
                commands = realloc(commands, buffer*sizeof(char*));
                if(!commands){
                    error_message();
                    exit(1);
                }
            }
        }
        token = strtok(NULL, "&");
    }

    //Set the last command as NULL, the command amount as the last position from the while loop and free the copied line memory
    commands[pos] = NULL;
    *commandAmount = pos;
    free(line_copy);
    return commands;
}

// This method parses commands into usable types
char **parse_commands(char *line){
    int buffer = BUFSIZE, pos=0;
    char **tokens = malloc(buffer * sizeof(char*));
    char *token;
    char *line_copy = strdup(line); // Line is again copied since strtok modifies the original

    if(!tokens || !line_copy){
        error_message();
        exit(1);
    }

    //Tokenize with delimiters that are defined at the top of the program
    token = strtok(line_copy, DELIMS);
    while (token != NULL){
        // Populate tokens arraylist with the tokens
        tokens[pos] = strdup(token);
        pos++;
        
        //Add memory to arraylist if required
        if(pos >= buffer){
            buffer += BUFSIZE;
            tokens = realloc(tokens, buffer*sizeof(char*));
            if(!tokens){
                error_message();
                exit(1);
            }
        }
        token = strtok(NULL, DELIMS);
    }

    //Set the last token as NULL and free the copied line memory
    tokens[pos] = NULL;
    free(line_copy);
    return tokens;
}

//Method for redirecting output to file
int redirect(char **args, char **fileName){
    int pos = -1;
    int count = 0;

    for(int i=0; args[i] != NULL; i++){
        if(strcmp(args[i], ">") == 0){
            pos = i;
            count++;
        }
    }

    //Multiple redirects, error
    if(count > 1){
        return -1;
    }

    //No redirect, return
    if(pos == -1){
        *fileName = NULL;
        return 0;
    }

    //No file name after >, error
    if(args[pos + 1] == NULL){
        return -1;
    }

    //Multiple files or other text after file name, error
    if(args[pos + 2] != NULL){
        return -1;
    }

    //file name is one argument after > and free memories of used args
    *fileName = strdup(args[pos + 1]);
    free(args[pos]);
    free(args[pos+1]);
    args[pos] = NULL;

    return 1; // Reached if redirection exists
}

//Funcation to search for an executable in search paths
char *search_executable_path(char *command){

    //loop to go through each path
    for(int i=0; i<path_amount && search_paths[i] != NULL; i++){
        int len = strlen(search_paths[i]) + strlen(command) + 2; //Add null terminator to path length
        char *path = malloc(len);
        if(!path){
            return NULL;
        }

        // Create the full path and check if it is an executable and return it if true
        snprintf(path, len, "%s/%s", search_paths[i], command);
        if(access(path, X_OK) == 0){
            return path;
        }

        free (path);
    }
    return NULL;
}

// Method for executing command
// Got help for the command execution from here: https://github.com/szholdiyarov/command-line-interpreter/blob/master/myshell.c
int execute_command(char **args){

    // No command provided
    if(args[0] == NULL){
        return 0;
    }

    //Built-in exit command
    if(strcmp(args[0], "exit") == 0){
        if(args[1] != NULL){
            error_message();
            return 1;
        }
        exit(0);
    }

    //Built-in cd command
    if(strcmp(args[0], "cd") == 0){
        
        //Error if no directory provided
        if(args[1] == NULL || args[2] != NULL){
            error_message();
            return 1;
        }

        //Change directory
        if(chdir(args[1]) != 0){
            error_message();
            return 1;
        }

        return 0;
    }

    //Built-in command path
    if(strcmp(args[0], "path") == 0){

        //free the current paths
        for(int i=0; i<100; i++){
            if(search_paths[i] != NULL){
                free(search_paths[i]);
                search_paths[i] = NULL;
            }
        }
        
        path_amount = 0;
        //Path amount is limited to 100 because the original search_paths array size is 100, should be enough
        for(int i=1; args[i] != NULL && path_amount < 100; i++){
            search_paths[path_amount] = strdup(args[i]);
            path_amount++;
        }
        return 0;
    }

    //Get info if result should be redirected to a file or not
    char *fileName;
    int redirect_result = redirect(args, &fileName);

    if(redirect_result == -1){
        error_message();
        return 1;
    }

    //find the executable using access() systen call
    char *executable_path = search_executable_path(args[0]);
    if(executable_path == NULL){
        error_message();
        return 1;
    }

    //fork created using pid for the actual execution of command, so multiple commands can be executed
    pid_t pid = fork();
    if(pid == 0){

        // Redirection is handled if it is detected, got help for redirecting stdout to a file from the man pages and here: https://gist.github.com/miguelmota/ec95bed91ecc19814ddcef1eb1389fa4
        if(redirect_result == 1){
            int file = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(file == -1){
                error_message();
                exit(1);
            }

            if(dup2(file, STDOUT_FILENO) == -1 || dup2(file, STDERR_FILENO) == -1){
                error_message();
                exit(1);
            }
            close(file);
        }

        //Actual execution with execv and a consequent error check, got background help from here: https://www.geeksforgeeks.org/c/exec-family-of-functions-in-c/
        int status_code = execv(executable_path, args);
        if(status_code == -1){
            error_message();
            exit(1);
        }

    } else if(pid < 0){
        error_message();
        free(executable_path);
        return 1;
    }

    free(executable_path);
    return pid;
}

//Method for printing the error message, a little faster and cleaner than writing the same thing at every error.
void error_message(){
    char error_message[30] = "An error has occurred.\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}