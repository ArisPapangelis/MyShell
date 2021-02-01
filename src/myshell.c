#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

void execute(char *, char *);		//The function that parses and executes each command. Called only through child processes.
void shell(char [], FILE *);				//The primary function of our program, that parses the string given from stdin and creates the appropriate child processes for the execution of commands.

int main(int argc, char *argv[]){
	
	//INTERACTIVE MODE.
	if (argc==1){						//If there is only one argument (the name of the program), then we are in interactive mode.
		char buffer[512];				//Maximum of 512 characters per line.
		printf("papaggelis_8883> ");	//Printing of the prompt.
		while (fgets(buffer,512,stdin)!=NULL){	//Input from stdin, by the shell user.	
			shell(buffer,NULL);					//Calling of our primary function, with the line read from stdin as an argument.
			printf("papaggelis_8883> ");	//Printing of the prompt.
		}
	}
	
	//BATCH MODE.
	else{								//If there are more than 1 arguments, the second argument is the name of our batchfile, according to spec.
		FILE *fp=fopen(argv[1],"r");	//Here, we open our batch file through fopen, saving the appropriate file pointer to it.
		char buffer[512];				//Maximum of 512 characters per line in the file.
		while (fgets(buffer,512,fp)!=NULL){		//We keep reading lines until we reach the end of file (EOF). In that case, fgets returns NULL.					
			shell(buffer,fp);						//Calling of our primary function, with the current line read from the file as an argument.
		}	
	}
	return 0;
}

void shell(char buffer[], FILE *fp){	

	char *copy = strdup(buffer);	//We duplicate the parameter buffer. This is needed in order to find which delimiter was actually used each time, since in buffer, delimiters are replaced with \0.
	char *token;					//The token contains a pointer to the start of each delimited string in the line. Modified through strtok.
	pid_t pid;						//Returned pid from parent and child.
	
	token = strtok(buffer, ";&\r\n");	//The first delimited string in the line.
	
	int status=100;						//Exit status of each child. Initial value is set to 100.					
	
	while (token != NULL) {													//We keep parsing the line, until we reach the end of it (EOF). In that case, NULL is returned and the execution stops.
		char *copyarg=strdup(token);	//Needed in the child to find which delimiter was used in the current token, > or <.
		pid=fork();			//Forking of parent and child.
		if (pid<0){
			fprintf(stderr,"Forking error, aborting...");	//Error message if the fork was unsuccessful.
			exit(1);
		}
		
		//Child process for pid=0. For successful execution, the child returns status 0. Elsewise, it returns status 1.
		else if (pid==0){
			if (fp!=NULL) fclose(fp);			
			if (copy[token-buffer-1]=='&'){			//Execution when the delimiter is &&.
				if (status==100){
					fprintf(stderr,"Cannot start command with delimiter.\n");	//The first command in the line cannot start with a delimiter.
					exit(1);
				}
				else if (copy[token-buffer-2]!='&' || copy[token-buffer-3]=='&'){		//Check to see if the delimiter is actually &&.
					fprintf(stderr,"Wrong delimiter. Try && as a delimiter.\n");	//If the delimiter was not actually &&, print the appropriate error message.
					exit(1);
				}
				else if (status==0) {				//If the previous command executed successfully, then we can execute the command that was delimited by &&.
					execute(token,copyarg);
					exit(0);
				}
				else {								//If the previous command was not executed successfully, then we abort the execution of the current command and display the appropriate error message.
					fprintf(stderr,"Previous command failed to execute, aborting the execution of '%s'\n",token);
					exit(1);
				}
			}
			
			else if (copy[token-buffer-1]==';'){	//Execution when the delimiter is ;.
				if (status==100){
					fprintf(stderr,"Cannot start command with delimiter.\n");	//The first command in the line cannot start with a delimiter.
					exit(1);
				}
				else if (copy[token-buffer-2]==';'){							//Check to see if the delimiter is actually ;.
					fprintf(stderr,"Wrong delimiter. Try ; as a delimiter.\n");	//If the delimiter was not actually ;, print the appropriate error message.
					exit(1);
				}
				else {
					execute(token,copyarg);		//Execution when the delimiter is ;, regardless of exit status.
					exit(0);
				}
			}
			
			else {								//Execution of the first command in the line.
				execute(token,copyarg);
				exit(0);
			}
		}
		//End of child process.
		
		
		//Back in parent process.
		pid=wait(&status);			//Wait for the child to finish first, before continuing execution. We do this in order to print the results of the commands in the correct order. (the order that the commands were given)
		if (WIFEXITED(status)){
			status=WEXITSTATUS(status);	//Get the exit status of the child.
			
			char *temptok=strdup(token);
			char *saveptr;
			char *tok=strtok_r(temptok,"|",&saveptr);	//We need the re-entrant version of strtok, since we are already in the middle of tokenizing the original token.
			int times=0;
			tok=strtok_r(NULL,"|",&saveptr);			//Skip the first command in the pipe since it was executed above.
			while (status==8 && tok!=NULL) {			//Status 8 means that a pipe was found in the command that was given.
				//fprintf(stderr,"%s\n",tok);
				char dest[50];
				strcpy(dest,tok);						//Copy the token into dest, so as not to modify the original token.
				if (fork()==0){							//Initialize a child process to make a copy of temp.txt (output file of the first command) and name the copy temp1.txt(input file of the following command).
					char *args[]={"cp","temp.txt","temp1.txt",NULL};	
					execvp(args[0],args);
				}
				wait(NULL);
				strcat(dest,"<temp1.txt");				//Get input for the next command in the pipe from temp1.txt.
				tok=strtok_r(NULL,"|",&saveptr);		//Advance to the next token delimited by | , if it exists.
				//fprintf(stderr,"%s\n",tok);
				if (tok!=NULL) {
					strcat(dest,"|a");					//If the token is null, we need to copy a | delimiter and a random character to the end of the dest string. This is 
				}										//kind of a "hack", but it is needed for our execute function to work correctly with pipes.
				copyarg=strdup(dest);
				
				//fprintf(stderr,"%s\n",dest);

				pid=fork();
				if (pid==0){
					execute(dest,copyarg);				//Initialize a child process to execute the next command in the pipe. 
					exit(0);
				}
				pid=wait(&status);
				status=WEXITSTATUS(status);				//Get the exit status of the child.
			}
			free(temptok);
			
			if (status==10){
				printf("Exiting...\n");		//Exit immediately if the child exited with exit status 2. That means that a quit command was issued.
				exit(0);
			}
		}
		
		token = strtok(NULL, ";&\r\n");	//Advance to the next part of the split string.
		free(copyarg);
	}
	free(copy);
}

void execute(char *token, char *copy){
	
	char *tokcopy=strdup(token);	//We duplicated the parameter token, since it is needed when parsing for > or <.
	char *tok=strtok(token,"|><");	//Delimit using > and <, to get the name of the command, as well as the parameters, if any exist, AFTER delimiting once more in the command below, this time with whitespace.
	tok=strtok(tok," \t");
	char *cmd=tok;					//The first token is the name of the command. Needed for the OS to know in what directory to search for the command.
	char *argv[100];				//We assume a maximum of 99 parameters per command. Can be changed if more are needed. (unlikely, but could easily be achieved with dynamic memory allocation)
	
	
	if (strcmp(tok,"quit")==0){		//If the command was "quit", exit immediately with status 2. The exit status is going to be intercepted in the parent process.
		exit(10);
	}
	
	int i=0;
	while (tok!=NULL){
		argv[i]=tok;				//We fill argv with the name of the command, as well as the command's parameters.
		tok=strtok(NULL," \t");
		i++;
	}
	argv[i]=NULL;					//Finally, we put NULL in the last position of argv, as that's what's needed if we are to pass it as an argument to execvp.
	
	
	int fdout=STDOUT_FILENO;			//Default outpout file descriptor.
	int fdin=STDIN_FILENO;				//Default input file descriptor.
		
	
	tok=strtok(tokcopy,"|><");		//Parsing for > or <.
	char delim = copy[tok-tokcopy+strlen(tok)];			//Next delimiter, needed for handling < and >.
	char *end;
	char *temptok;
	int piping=0;
	while (delim=='>' || delim=='<' || delim=='|'){			//Redirecting input and output from or to a file, respectively, or redirecting to another command (pipe).
		tok = strtok(NULL, "|><");				//Advance to the next token.
		while (*tok==' ' || *tok=='\t'){
			tok++;								//Here, we have to discard leading whitespace characters, or else we will not be able to open the specified file below.
		}
		
		temptok=strdup(tok);					//The token is duplicated, since we do not want to modify the original token.
		end=temptok+strlen(temptok)-1;
		while(end>temptok && (*end==' ' || *end=='\t')){
			end--;								//Here, we have to discard trailing whitespace characters, or else we will not be able to open the specified file below.
		}
		end++;
		*end='\0';								//We null terminate the duplicated string, and then pass it as an argument in the open system call.
		
		if (delim=='>'){
			fdout = open(temptok,O_WRONLY | O_APPEND | O_CREAT,S_IRWXU);	//If the delimiter is >, we change the original file descriptor to the one specified by our command.
		}
		else if (delim=='<'){
			fdin = open(temptok,O_RDONLY | O_CREAT,S_IRWXU);				//If the delimiter is <, we change the original file descriptor to the one specified by our command.
		}
		else if (delim=='|'){
			fdout=open("temp.txt",O_WRONLY | O_TRUNC | O_CREAT,S_IRWXU);	//If the delimiter is |, we change the original file descriptor to a temporary file that is going to be read by the parent.
			piping=1;														//Also, we set the piping flag to 1.
		}
		free(temptok);
		delim = copy[tok-tokcopy+strlen(tok)];			//This sequence loops until the delimiter is not < or > or | any more.
	}
	
	dup2(fdout,STDOUT_FILENO);		//Stdout is now the fdout that was parsed above.
	dup2(fdin,STDIN_FILENO);		//Stdin is now the fdin that was parsed above.
	
	free(tokcopy);
	
	if (piping==1){					//This snippet is only executed if there is a pipe between two commands.
		if(fork()==0){				//We create a child of the child process, since in case of successful execution, we still need to return status 8 to the parent process.
			if (execvp(cmd,argv)==-1) {
				fprintf(stderr,"The command '%s' does not exist. Execution aborted.\n",cmd);		//Print an appropriate error message if the command failed to execute.
				exit(1);	//Exit with status 1 (failed execution).
			}
		}
		wait(NULL);
		exit(8);					//Exit with status 8, to signify to the parent that the output of the executed command will be piped to the next command.
	}
	
	
	if (execvp(cmd,argv)==-1) {
		fprintf(stderr,"The command '%s' does not exist. Execution aborted.\n",cmd);		//Print an appropriate error message if the command failed to execute.
		exit(1);	//Exit with status 1 (failed execution).
	}
}
