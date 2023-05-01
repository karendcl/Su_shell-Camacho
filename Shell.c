#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <stdbool.h>

#define TRUE 0
#define FALSE 1

// Shell pid, pgid, terminal modes
static pid_t suchel_PID;
static pid_t suchel_PGID;
static int suchel_IS_INTERACTIVE;
static struct termios suchel_TMODES;

static char* currentDirectory;
extern char** environ;

struct sigaction act_child;
struct sigaction act_int;

int no_reprint_prmpt;
int isPiping = 0;

pid_t pid;


// signal handler for SIGCHLD */
void signalHandler_child(int p);
// signal handler for SIGINT
void signalHandler_int(int p);

int changeDirectory(char * args[]);

int suchel_true(){
    return 0;
}

int suchel_false(){
    return 1;
}

#define MAX_SIZE_CMD 1024
#define PIPE_FILE "pipe_file"
#define MAX_HISTORY_SIZE 10

char history[MAX_HISTORY_SIZE][MAX_SIZE_CMD];
int history_count = 0;

void init() {
    // See if we are running interactively
    suchel_PID = getpid();

    // The shell is interactive if STDIN is the terminal
    suchel_IS_INTERACTIVE = isatty(STDIN_FILENO);

    if (suchel_IS_INTERACTIVE) {
        // Loop until we are in the foreground
        while (tcgetpgrp(STDIN_FILENO) != (suchel_PGID = getpgrp()))
            kill(suchel_PID, SIGTTIN);


        // Set the signal handlers for SIGCHILD and SIGINT
        act_child.sa_handler = signalHandler_child;
        act_int.sa_handler = signalHandler_int;


        sigaction(SIGCHLD, &act_child, 0);
        sigaction(SIGINT, &act_int, 0);

        // Put ourselves in our own process group
        setpgid(suchel_PID, suchel_PID); // we make the shell process the new process group leader
        suchel_PGID = getpgrp();
        if (suchel_PID != suchel_PGID) {
            printf("Error, the shell is not process group leader");
            exit(EXIT_FAILURE);
        }
        // Grab control of the terminal
        tcsetpgrp(STDIN_FILENO, suchel_PGID);

        // Save default terminal attributes for shell
        tcgetattr(STDIN_FILENO, &suchel_TMODES);

        // Get the current directory that will be used in different methods
        currentDirectory = (char *) calloc(1024, sizeof(char));
    } else {
        printf("Could not make the shell interactive.\n");
        exit(EXIT_FAILURE);
    }
}

void signalHandler_child(int p) {
    /* Wait for all dead processes.
     * We use a non-blocking call (WNOHANG) to be sure this signal handler will not
     * block if a child was cleaned up in another part of the program. */
    while (waitpid(-1, NULL, WNOHANG) > 0) {}

}

void signalHandler_int(int p) {
    // We send a SIGTERM signal to the child process
    if (kill(pid, SIGTERM) == 0) {
        printf("\nProcess %d received a SIGINT signal\n", pid);
        no_reprint_prmpt = 1;
    } else {
        printf("\n");
    }
}

char* replace(char* input, int pos){
    char* output = (char*) malloc(1024*sizeof(char));
    int i =0;
    for ( i = 0; i < pos; i++)
    {
        output[i] = input[i];
    }
    
    //get next character after again
    int j = pos + 5;
    while (input[j] == ' ' || input[j] == '\t'){
        j++;
    }
    //get the number after again
    //check if input j is a number
    if (isdigit(input[j]) == 0){
        return input;
    }
    int k = atoi(&input[j]);

    //get the command from history
    if (k > history_count || k < 1){
        printf("Wrong index\n");
        return input;
    }
    char* command = history[k-1];
    //copy the command to output
    for (int i = 0; i < strlen(command); i++)
    {
        output[i+pos] = command[i];
    } 
    //copy the rest of the input to output


    
    for (int k = i+j; k < strlen(input); k++)
    {
        output[k] = input[++j];
    }
    return output;
    
}

char* parse_again(char* input){
    //replace the word again by history[i]
    char* output = (char*) malloc(1024*sizeof(char));

    bool found = true;
    int i =0;

    printf("%s", input);

    if (strlen(input) <= 5){
        printf("Is returning input\n");
        return input;
    }
    
    //search for the word again in input
    for ( i = 0; i < strlen(input)-5; i++)
    {
        printf("%c", input[i]);
        //if the word again starts at input[i]
        if (input[i] == 'a' && input[i+1] == 'g' && input[i+2] == 'a' && input[i+3] == 'i' && input[i+4] == 'n')
        {
            found = true;
            output = replace(input, i);
        }

    }

    if (found == false){
        return input;
    }
    return output;

}

char* add_spaces(char* input) {
    int len = strlen(input);
    int separator_count = 0;  // Count of separators encountered
    for (int i = 0; i < len; i++) {
        if (input[i] == '|' || input[i] == '>' || input[i] == '<' || input[i] == '&' || input[i] == ';') {
            if (input[i] == '>' && i < len-1 && input[i+1] == '>') {  // Found ">>" separator
                i++;
            }
            if (input[i] == '&' && i < len-1 && input[i+1] == '&') {  // Found && separator
                i++;
            }
            if (input[i] == '|' && i < len-1 && input[i+1] == '|') {  // Found ">>" separator
                i++;
            }
            separator_count++;
        }
    }
    if (separator_count == 0) {  // No separators found, return original string
        return input;
    }
    char* output = (char*) malloc((len+2*separator_count+1)*sizeof(char));  // Allocate space for output string
    int pos = 0;  // Position in output string
    for (int i = 0; i < len; i++) {
        if (input[i] == '|' || input[i] == '>' || input[i] == '<' || input[i] == '&' || input[i]==';' || input[i]=='#') {  // Found a symbol
            if (input[i] == '>' && i < len-1 && input[i+1] == '>') {  // Found ">>" separator
                i++;
                if (pos > 0 && output[pos-1] != ' ') {  // Add space before symbol if needed
                    output[pos++] = ' ';
                }
                output[pos++] = '>';  // Add ">>" symbol to output string
                output[pos++] = '>';
                if (i < len-1 && input[i+1] != ' ') {  // Add space after symbol if needed
                    output[pos++] = ' ';
                }
            } 
             else if (input[i] == '&' && i < len-1 && input[i+1] == '&') {  // Found && separator
                i++;
                if (pos > 0 && output[pos-1] != ' ') {  // Add space before symbol if needed
                    output[pos++] = ' ';
                }
                output[pos++] = '&';  // Add && symbol to output string
                output[pos++] = '&';
                if (i < len-1 && input[i+1] != ' ') {  // Add space after symbol if needed
                    output[pos++] = ' ';
                }
            }
            else if (input[i] == '|' && i < len-1 && input[i+1] == '|') {  // Found || separator
                i++;
                if (pos > 0 && output[pos-1] != ' ') {  // Add space before symbol if needed
                    output[pos++] = ' ';
                }
                output[pos++] = '|';  // Add || symbol to output string
                output[pos++] = '|';
                if (i < len-1 && input[i+1] != ' ') {  // Add space after symbol if needed
                    output[pos++] = ' ';
                }
            }
            
            else {  // Found a single separator
                if (pos > 0 && output[pos-1] != ' ') {  // Add space before symbol if needed
                    output[pos++] = ' ';
                }
                output[pos++] = input[i];  // Add symbol to output string
                if (i < len-1 && input[i+1] != ' ') {  // Add space after symbol if needed
                    output[pos++] = ' ';
                }
            }
        } else {
            //if is a space and next is a space, skip
            if (input[i] == ' ' && input[i-1] == ' '){
                continue;
            }

            output[pos++] = input[i];  // Copy character to output string
        }
    }
    output[pos] = '\0';  // Add null terminator to output string
    return output;
} 

int suchel_cd(char **args);

int suchel_help(char **args);

int suchel_exit(char **args);

int suchel_history(char **args);

int suchel_again(char **args);

int suchel_if(char **args);

int suchel_execute(char **args);

int suchel_if(char **args);

char *builtin_str[] = {
        "cd",
        "help",
        "exit",
        "history",
        "again",
        "true",
        "false",
        "if"
};

int (*builtin_func[])(char **) = {
        &suchel_cd,
        &suchel_help,
        &suchel_exit,
        &suchel_history,
        &suchel_again,
        &suchel_true,
        &suchel_false,
        &suchel_if
};

int suchel_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}


int suchel_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Camacho: expected argument to \"cd\"\n");
        return 0;
    } else {
        if (chdir(args[1]) != 0) {
             perror("could not find the directory");
             return 0;
        }
    }
    return 0;
}

int suchel_if(char**args){
    //iterate through args and copy the condition
    int i=0;

    int cant_end = 0;
    int cant_if = 0;
    int cant_then = 0;
    int cant_else = 0;
    int pend = 0;
    int pthen =0;
    int pelse = 0;

    while(args[i] != NULL){
        if(strcmp(args[i], "end") == 0){
            pend = i;
            cant_if--;
            cant_then--;
        }
        if(strcmp(args[i], "if") == 0){
            cant_if++;
        }
        if(strcmp(args[i], "then") == 0){
            cant_then++;
                if ((cant_then==cant_if) && (cant_if==1)){
                    pthen = i; //where the then command starts
                    }
        }

        if(strcmp(args[i], "else") == 0){
                if (cant_if==1){ //if i find an else command and I only have an open if command
                    pelse = i; //where the else command starts
                    }
        }
        
        i++;
    }

    if (cant_if != 0){
        printf("Error: if command not closed \n");
        return FALSE;
    }

    if (pelse==0){
        pelse = pend;
    }

    //copy into condition array from 0 to pthen
    char* condition[100];
    int j = 1;
    while(j < pthen){
        condition[j-1] = args[j];
        j++;
    }
    condition[j] = NULL;

    //copy into then array from pthen+1 to pelse
    char* then[100];
    j = pthen + 1;
    int k = 0;
    while(j < pelse){
        then[k] = args[j];
        j++;
        k++;
    }
    then[k] = NULL;

    //copy into else array from pelse+1 to pend
    char* elsee[100];
    j = pelse + 1;
    k = 0;
    while(j < pend){
        elsee[k] = args[j];
        j++;
        k++;
    }
    elsee[k] = NULL;

    //execute the condition
    int condition_result = suchel_execute(condition);

    //execute the then or the else
    if(condition_result == 0){
        return suchel_execute(then);
    }else{
        return suchel_execute(elsee);
    }

    return TRUE;
}

int suchel_help(char **args) {

    //read from help.txt
    char hh[256] = "h";
     FILE *fp;
    char str[1000];

    if (args[1]== NULL || args[1]=="")
    {
        fp = fopen("Shell help.txt", "r");
    }
    else
    {
        char command[256];
        strcpy(command, args[1]);

        //concat two strings 
        strcat(hh, command);
        strcat(hh, ".txt");

        fp = fopen(hh, "r");
    }

    if (fp== NULL){
            printf("Could not open file \n");
            return FALSE;
        }

    
    while (fgets(str,1000,fp) != NULL) printf("%s", str);
    fclose(fp);
    printf("\n");
    return TRUE;

}

int suchel_exit(char **args) {
    return -1;
}

int suchel_history(char **args) {

    //display the array
    for (int i = 0; i < history_count; i++) {
        printf("%d %s\n", i+1, history[i]);
    }

    return TRUE;
}

void load_history_from_array(){
    FILE *fp;
    //we open the txt in w mode to clean it
    fp = fopen("Shell history.txt", "w");
    fclose(fp);
    fp = fopen("Shell history.txt", "a");

    for (int i = 0; i < history_count; i++)
    {
        fprintf(fp, "%s\n", history[i]);
    }
    
    fclose(fp);
}

void load_history_from_txt(){
    FILE *fp;
    char line[1000];
    int i = 0;
    fp = fopen("Shell history.txt","r");
    if (fp==NULL){
        printf("Error");
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (isspace(*line)) continue;
        strcpy(history[i], line);
        i++;
    }

    history_count = i;
    

}

void save_history(char* command) {
    
    if (isspace(*command)) return;

    //check if command starts with again
    char* tokens[MAX_SIZE_CMD];

    char* command_copy = (char*) malloc(strlen(command) + 1);
    strcpy(command_copy, command);

    char* command_copy1 = (char*) malloc(strlen(command) + 1);
    strcpy(command_copy1, command);

    tokens[0] = strtok(command_copy1, " ");

    if (strcmp(tokens[0], "again") == 0) return;


    // If history is full, shift all commands to the left and discard the oldest one
    if (history_count == MAX_HISTORY_SIZE) {
        for (int i = 0; i < MAX_HISTORY_SIZE - 1; i++) {
            strcpy(history[i], history[i+1]);
        }
        history_count--;
    }

    // Add the new command to the end of history
    strcpy(history[history_count], command_copy);
    history_count++;

    load_history_from_array();

}

int suchel_again(char** command_parts) {
    if (command_parts[1] == NULL) {
        printf("Error: again command requires an index\n");
        return FALSE;
    }

    int index = atoi(command_parts[1]);
    int numTokens = 0;

    if (index > 0 && index <= history_count) {
        char* command = (char*) malloc(strlen(history[index-1]) + 1) ;
        strcpy(command, history[index-1]);

        //copy *command into an array
        char line[MAX_SIZE_CMD];
        strcpy(line, command);



        // char* command_copy = (char*) malloc(strlen(command) + 1);
        // strcpy(command_copy, command);

        char* tokens[MAX_SIZE_CMD];
        save_history(command);
       

        //printf("Executing command: %s\n", command_copy);
        printf("Executing command: %s\n", command);

        // char* command_copy1 = (char*) malloc(strlen(command_copy) + 1);
        // strcpy(command_copy1, command_copy);

        // if ((tokens[0] = strtok(command_copy, " \n\t")) == NULL) return 0;

        //char comm[MAX_SIZE_CMD];
        //strcpy(comm, HandleCommand(line) );

        if ((tokens[0] = strtok(line, " \n\t")) == NULL) return FALSE;
        numTokens = 1;
        while ((tokens[numTokens] = strtok(NULL, " \n\t")) != NULL) numTokens++;

        int status = suchel_execute(tokens);
        return status;
    } else {
        printf("Invalid command index\n");
        return FALSE;
    }

    return TRUE;
    
}

/* ============================================================================================ */

void RedirectOutput(char *outputFile, int fileDescriptor) {
    fileDescriptor = open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    dup2(fileDescriptor, STDOUT_FILENO);
    close(fileDescriptor);
}

void RedirectInput(char *inputFile, int fileDescriptor) {
    fileDescriptor = open(inputFile, O_RDONLY, 0600);
    dup2(fileDescriptor, STDIN_FILENO);
    close(fileDescriptor);
}

void RedirectAppendOutput(char *outputFile, int fileDescriptor) {
    fileDescriptor = open(outputFile, O_CREAT | O_APPEND | O_WRONLY, 0600);
    dup2(fileDescriptor, STDOUT_FILENO);
    close(fileDescriptor);
}

void RedirectPipeOutput(int *pipeFd) {
    dup2(pipeFd[1], STDOUT_FILENO);
    close(pipeFd[0]);
    close(pipeFd[1]);
    no_reprint_prmpt = 1;
}

int HandlePipeAndPipeOutput(int *pipeFd, char **pipeArgs, int option, char *outputFile, int fileDescriptor) {
    int err = -1;
    pid_t pid2;

    // execute the piped command
    if ((pid2 = fork()) == -1) {
        printf("Child process could not be created\n");
        return -1;
    }

    if (pid2 == 0) {
        // redirect input from pipe
        dup2(pipeFd[0], STDIN_FILENO);
        close(pipeFd[0]);
        close(pipeFd[1]);

        if (option == 1) {
            RedirectOutput(outputFile, fileDescriptor);
//            printf("Got to option 1");
        }

        if (execvp(pipeArgs[0], pipeArgs) == -1) {
            printf("err");
            kill(getpid(), SIGTERM);
            return -1;
        }
    }

    no_reprint_prmpt = 0;

    close(pipeFd[0]);
    close(pipeFd[1]);
    waitpid(pid2, NULL, 0);

    return 0;
}

int suchel_launch(char **args, char *inputFile, char *outputFile, int option) {
    int err = -1;
    int fileDescriptor;

    if ((pid = fork()) == -1) {
        printf("Child process could not be created\n");
        return -1;
    }

    if (pid == 0) {
        if (isPiping == 1) {
            RedirectInput(PIPE_FILE, fileDescriptor);
            isPiping = 0;
        }

        //
        if (option == 1) {
            RedirectOutput(outputFile, fileDescriptor);
        } else if (option == 2) {
            RedirectInput(inputFile, fileDescriptor);
        } else if (option == 3) {
            RedirectAppendOutput(outputFile, fileDescriptor);
        } else if (option == 5) {
            RedirectInput(inputFile, fileDescriptor);
            RedirectOutput(outputFile, fileDescriptor);
        } else if (option == 7) {
            RedirectInput(inputFile, fileDescriptor);
            RedirectAppendOutput(outputFile, fileDescriptor);
        }

        if (execvp(args[0], args) == err) 
        {
            printf("error");
            kill(getpid(), SIGTERM);
            return -1;
        }
    }

    waitpid(pid, NULL, 0);

    return TRUE;
}

int suchel_parsing(char **commands, char **separators, int numCommands, int numSeparators) {
    int executed = 0;
    int currSeparator = 0;
    int start = 0;
    int buffer = 0;
    char *buffer_files[] = {"buffer_file1", "buffer_file2"};

    for (int i = 0; i < numCommands; i++) {
        if (executed == 0) {
            start = i;
            if (separators[currSeparator] != NULL &&
                (strcmp(separators[currSeparator], ">") == 0 || strcmp(separators[currSeparator], ">>") == 0)) {
                while (commands[i++] != NULL);
                if (commands[i] == NULL) {
                    printf("Not enough arguments \n");
                    return FALSE;
                }

                if (strcmp(separators[currSeparator], ">") == 0) {
                    suchel_launch(commands + start, NULL, commands[i], 1);
                } else {
                    suchel_launch(commands + start, NULL, commands[i], 3);
                }

                currSeparator++;
            } else if (separators[currSeparator] != NULL && strcmp(separators[currSeparator], "<") == 0) {
                while (commands[i++] != NULL);
                if (commands[i] == NULL) {
                    printf("Not enough arguments \n");
                    return FALSE;
                }

                if (separators[currSeparator + 1] != NULL && strcmp(separators[currSeparator + 1], ">") == 0) {
                    suchel_launch(commands + start, commands[i], commands[i + 2], 5);
                    i += 2;
                    currSeparator++;
                } else if (separators[currSeparator + 1] != NULL && strcmp(separators[currSeparator + 1], "|") == 0) {
                    suchel_launch(commands + start, commands[i], PIPE_FILE, 5);
                    isPiping = 1;
                    currSeparator++;
                } else if (separators[currSeparator + 1] != NULL && strcmp(separators[currSeparator + 1], ">>") == 0) {
                    suchel_launch(commands + start, commands[i], commands[i + 2], 7);
                    i += 2;
                    currSeparator++;
                } else {
                    suchel_launch(commands + start, commands[i], NULL, 2);
                }

                currSeparator++;
            } else if (separators[currSeparator] != NULL && strcmp(separators[currSeparator], "|") == 0) {
                while (commands[i++] != NULL);
                if (commands[i] == NULL) {
                    printf("Not enough arguments \n");
                    return FALSE;
                }

                do {
                    if(currSeparator > 0 && strcmp(separators[currSeparator - 1], "|") == 0){
                        suchel_launch(commands + start, buffer_files[buffer ^ 1], buffer_files[buffer], 5);
                    }
                    else{
                        suchel_launch(commands + start, NULL, buffer_files[buffer], 1);
                    }
//                    suchel_launch(commands + start,
//                               (currSeparator > 0 && strcmp(separators[currSeparator - 1], "|") == 0) ? buffer_files[
//                                       buffer ^ 1] : NULL,
//                               buffer_files[buffer], 1);

                    // Advance to the next command
                    start = i++;
                    while (commands[i++] != NULL);

                    // Swap the buffer index
                    buffer ^= 1;
                    currSeparator++;


                } while (separators[currSeparator] != NULL && strcmp(separators[currSeparator], "|") == 0);

//                printf("%s", separators[currSeparator]);

                // Execute the last command in the pipe chain

                if(separators[currSeparator] != NULL &&
                    (strcmp(separators[currSeparator], ">") == 0 || strcmp(separators[currSeparator], ">>") == 0)){

                    while (commands[i++] != NULL);
                    i-=2;

                    if (commands[i] == NULL) {
                        printf("Not enough arguments \n");
                        return FALSE;
                    }

                    if (strcmp(separators[currSeparator], ">") == 0) {
                        suchel_launch(commands + start, buffer_files[buffer ^ 1], commands[i++], 5);
                    } else {
                        suchel_launch(commands + start, buffer_files[buffer ^ 1], commands[i++], 7);
                    }

                    currSeparator++;
                }
                else{
                    suchel_launch(commands + start, buffer_files[buffer ^ 1], NULL, 2);
                }

                currSeparator++;
                i -= 2;
            } else {
                suchel_launch(commands + start, NULL, NULL, 0);
            }

            // Mark as executed
            executed = 1;
        } else if (commands[i] == NULL) {
            // Unmark as executed
            executed = 0;
        }
    }

    // Remove the buffer files after the execution
    remove(buffer_files[0]);
    remove(buffer_files[1]);
    remove(PIPE_FILE);
    return TRUE;
}


int suchel_execute(char **args) {

    char **commands = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
    char **separators = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
    int numCommands = 0;
    int numSeparators = 0;

    bool chained = false; 


    //if args contains a || && ; | > < >> separate it for 
    
    

    //check if args contains ; and separate the commands
    for (int i = 0; args[i]!= NULL; i++)
    {
        if (strcmp(args[i],";")==0){
            //separate the commands by ; and execute them
            chained = true;
            char **args1 = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
            char **args2 = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
            int j = 0;
            while (strcmp(args[j],";")!=0){
                args1[j] = args[j];
                j++;
            }
            args1[j] = NULL;
            j++;
            int k = 0;
            while (args[j]!=NULL){
                args2[k] = args[j];
                j++;
                k++;
            }
            args2[k] = NULL;

            if (strstr(args1[0],"exit")!=NULL){
               return -1;
            }

            suchel_execute(args1);

            if (strstr(args2[0],"exit")!=NULL){
               return -1;
            }
            suchel_execute(args2);
        }
    }


    //check if args contains && and separate the commands
    for (int i = 0; args[i]!= NULL; i++)
    {
        if (strcmp(args[i],"&&")==0){
            chained = true;
            //separate the commands by ; and execute them
            char **args1 = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
            char **args2 = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
            int j = 0;
            while (strcmp(args[j],"&&")!=0){
                args1[j] = args[j];
                j++;
            }
            args1[j] = NULL;
            j++;
            int k = 0;
            while (args[j]!=NULL){
                args2[k] = args[j];
                j++;
                k++;
            }
            args2[k] = NULL;
            int p = suchel_execute(args1);
            if (p == TRUE){
                return suchel_execute(args2);
            }
            else return p;
            
        }
    }

    //check if args contains || and separate the commands
    for (int i = 0; args[i]!= NULL; i++)
    {
        if (strcmp(args[i],"||")==0){
            chained = true;
            //separate the commands by ; and execute them
            char **args1 = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
            char **args2 = malloc(sizeof(char *) * (MAX_SIZE_CMD + 1));
            int j = 0;
            while (strcmp(args[j],"||")!=0){
                args1[j] = args[j];
                j++;
            }
            args1[j] = NULL;
            j++;
            int k = 0;
            while (args[j]!=NULL){
                args2[k] = args[j];
                j++;
                k++;
            }
            args2[k] = NULL;
            int p = suchel_execute(args1);
            if (p !=TRUE){
                suchel_execute(args2);
            }
            else return TRUE;
            
        }
    }

    if (chained == false){
    for (int j = 0; j < suchel_num_builtins(); ++j) {
        if (strcmp(args[0], builtin_str[j]) == 0) {
            return (*builtin_func[j])(args);
        }
    }


    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], "|") == 0 || strcmp(args[i], ">>") == 0)   
        {
            separators[numSeparators++] = args[i];
            commands[numCommands++] = NULL; // mark end of previous command
        } else if (strcmp(args[i], "#") == 0) 
        {
            break; //ignore the rest of the command
        } else 
        {
            commands[numCommands++] = args[i];
        }
    }
    commands[numCommands] = NULL; // mark end of last command

    return suchel_parsing(commands, separators, numCommands, numSeparators);

    }

    free(commands);
    free(separators);
    return TRUE;
}



void suchel_loop() {
    char line[MAX_SIZE_CMD];
    char *newline;
    char *temp;
    char *tokens[MAX_SIZE_CMD];
    int numTokens;
    int status = TRUE;
    pid = -10;
    init();


    do {

        printf("Camacho \033[33m$ \033[32m>>>>> \033[0m");
        memset(line, '\0', MAX_SIZE_CMD);

        fgets(line, MAX_SIZE_CMD, stdin);

        

        temp = add_spaces(line);
        //temp = parse_again(temp);



        save_history(temp);

        //printf("%s\n", temp);

        temp[strcspn(temp, "\n")] = 0;
        if ((tokens[0] = strtok(temp, " \n\t")) == NULL) continue;

        numTokens = 1;
        while ((tokens[numTokens] = strtok(NULL, " \n\t")) != NULL) numTokens++;

        status = suchel_execute(tokens);

    } while (status != -1);
}


int main() {
    load_history_from_txt();
    suchel_loop();
    return EXIT_SUCCESS;
}



