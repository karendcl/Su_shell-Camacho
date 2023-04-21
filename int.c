#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>

#define	MAX_SIZE_CMD	256
#define	MAX_SIZE_ARG	16

char cmd[MAX_SIZE_CMD];				
// string holder for the command


char *argv[MAX_SIZE_ARG];			
// an array for command and arguments


pid_t pid;							
// global variable for the child process ID

char i;								
// global for loop counter

void get_cmd();						// get command string from the user
void convert_cmd();					// convert the command string to the required format by execvp()
void c_shell();						// to start the shell
void log_handle(int sig);			// signal handler to add log statements
void built_in_cmd();                // to handle built-in commands
void handle_cmd();

//declare a string array
char *built_in[3] = { "cd", "help"};


int main(){
	// tie the handler to the SGNCHLD signal
	signal(SIGCHLD, log_handle);

	// start the shell
	c_shell();

	return 0;
}

void c_shell(){
	while(1){
		// get the command from user
		get_cmd();

		// bypass empty commands
		if(!strcmp("", cmd)) continue;

        // fit the command into *argv[]
		convert_cmd();

		// check for built-in commands
        for(k=0; k<2; k++){
            if(!strcmp(built_in[k], argv[0])){
                built_in_cmd();
                break;
            }
        }

        if(!strcmp("exit", argv[0])){
            break;}

		

		
	}
}

void handle_cmd(){
    // fork and execute the command
		pid = fork();

		if(-1 == pid){
			printf("failed to create a child\n");
		}
		else if(0 == pid){
			// printf("hello from child\n");
			// execute a command
			execvp(argv[0], argv);
		}
		else{
			// printf("hello from parent\n");
			// wait for the command to finish if "&" is not present
			if(NULL == argv[i]) waitpid(pid, NULL, 0);
		}
}

void built_in_cmd(){
    // check for "cd" command
    if(!strcmp("cd", argv[0])){
        if(argv[1] == NULL){
            printf("No directory specified.\n");
        }else{
            if( chdir(argv[1]) != 0){
                printf("No such directory.\n");
            }
            else{
                //change directory

            }
        }
    }

    // check for "help" command
    if(!strcmp("help", argv[0])){
        printf("cd\t\tChange directory.\n");
        printf("exit\t\tExit the shell.\n");
        printf("help\t\tDisplay this help.\n");
    }

    
    
}

void get_cmd(){
	// get command from user
	printf("Camacho $\t");
	fgets(cmd, MAX_SIZE_CMD, stdin);
	// remove trailing newline
	if ((strlen(cmd) > 0) && (cmd[strlen (cmd) - 1] == '\n'))
        	cmd[strlen (cmd) - 1] = '\0';
	//printf("%s\n", cmd);
}

void convert_cmd(){
	// split string into argv
	char *ptr;
	i = 0;
	ptr = strtok(cmd, " ");
	while(ptr != NULL){
		//printf("%s\n", ptr);
		argv[i] = ptr;
		i++;
		ptr = strtok(NULL, " ");
	}

	// check for "&"
	if(!strcmp("&", argv[i-1])){
	argv[i-1] = NULL;
	argv[i] = "&";
	}else{
	argv[i] = NULL;
	}
	//printf("%d\n", i);
}

void log_handle(int sig){
	//printf("[LOG] child proccess terminated.\n");
	FILE *pFile;
        pFile = fopen("log.txt", "a");
        if(pFile==NULL) perror("Error opening file.");
        else fprintf(pFile, "[LOG] child proccess terminated.\n");
        fclose(pFile);
}