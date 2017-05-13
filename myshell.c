/*
 *Student name and No.: LAI CHEUK HIN 3035186152
 *Devlelopment platform: gedit
 *Last modified date: 22/10/2016
 *Compilation: execute 'gcc myshell.c -o myshell -Wall' in the Linux terminal
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

char command_line[255]; //String to get user input

void sigint_handler(int signum) {
	//clear the command line and then open a new line in the termination
	memset(command_line,0,sizeof(command_line));
	printf("\n");
}



int main() {

	int sigchldProcessID; //a variable for getting the process ID to SIGCHLD handler

	//SIGCHLD handler
	void sigchld_handler(int signum, siginfo_t *sig, void *v) {

		printf("\n[%d] Done\n",sigchldProcessID);

	}	

	//variables for extraction from /proc/pid/stat
	char str[50];
	FILE * file;
	int z;
	unsigned long h, ut, st;
	
	char * seperator, ** commands[5];	//variables used in the user input.
	
	while(1){ //keep looping the user input process 
		
		//reset the command_counter which is used to count commands (indicating the number of pipe needed)
		int command_counter = 0;

		//set the SIGINT handler
		struct sigaction sa;
		sigaction(SIGINT, NULL, &sa);
		sa.sa_handler = sigint_handler;
		sigaction(SIGINT, &sa, NULL);
		
		printf("## myshell $ ");
		fgets(command_line,255,stdin);
		
		if(strlen(command_line) <=1){	//if nothing in the user input, skip the loop and start again
			continue;
		}

		seperator = strtok(command_line," \t\n"); 	//get the first string from the command line

		int counter=0; 
		int i;
		
		int background_detected=0, background_error = 0, isTimeX = 0; //indicators of statuses

		for(i=0;i<5;i++)

			commands[i] = malloc(sizeof(char*)*255);

		if(strcmp(seperator,"exit")==0) { 	//if the first argument of the command input is "exit"
				
			seperator = strtok(NULL," \t\n");
			
			if(seperator!=NULL){  //if there is any arguments following "exit"
				printf("myshell: \"exit\" with other arguments!!!\n");
				continue;
			}
			else {
				printf("myshell: Terminated\n");	
				break;
			}
		}

		if(strcmp(seperator,"timeX")==0) {	//if the first argument of the command input is "timeX"
				
			isTimeX = 1; //set the indicator
			
			seperator = strtok(NULL," \t\n");
			if (seperator==NULL){ //if nothing is following "timeX"
				printf("myshell: \"timeX\" cannot be a standlone command\n");
				continue;
			}
		}

		while (seperator!=NULL){
		
			if(strcmp(seperator,"|")==0){ //if the string is "|" 
			
				commands[command_counter][counter]=NULL; //set the end of the current command set
				command_counter++; //number of pipe added
				counter = 0; //reset the counter of arguments in a command set
				
			}
			else if(strcmp(seperator,"&")==0){ //if the string is "&" meaning the process will be run in background
				background_detected = 1;	//set the indicator
				commands[command_counter][counter] = seperator;
				counter++;
				}
			else{
				if (background_detected){ 	//if new arguments is coming while "&" is existing
					background_error = 1;	//set the error indicator
					printf("myshell: '&' should not appear in the middle of the command line\n");
					break;
				} else {
					if(seperator[strlen(seperator)-1]=='|'){ //check whether the "|" is stuck with the last argument
						seperator[strlen(seperator)-1] = 0;
						commands[command_counter][counter] = seperator;
						commands[command_counter][counter+1] =NULL;
						command_counter++;
						counter = 0;
					}else{
						commands[command_counter][counter] = seperator;	//save the argument into the command set
						counter++;
						}
					}
			}
			seperator = strtok(NULL," \t\n");	//get the next argument
		}

		if (background_error == 1)
			continue;

		if (isTimeX && background_detected){
			printf("myshell: \"timeX\" cannot be run in background mode\n");
			continue;
		}

		commands[command_counter][counter]=NULL;	//set the last argument of the last command set be NULL

		int background=0;	//indicator used to set the process whether run in background or not
		
		if(commands[command_counter][counter-1][strlen(commands[command_counter][counter-1])-1] == '&'){ 
			//if the last character of the last argument is &
		
			background=1;
			
			if(strcmp(commands[command_counter][counter-1],"&")==0)	//check whether the "&" is stuck with the last argument
				commands[command_counter][counter-1] = NULL;
			else
				commands[command_counter][counter-1][strlen(commands[command_counter][counter-1])-1] = 0;
				
		}
		
		pid_t process_id = fork();
		
		//set the SIGINT handler to ignore the signal
		struct sigaction sa2;
		sigaction(SIGINT, NULL, &sa2);
		sa2.sa_handler = SIG_IGN;
		sigaction(SIGINT, &sa2, NULL);
		

		if(process_id==0){	//in the child process
			
			pid_t background_id = fork();	//fork one more grandchild process for running the command
			if(background_id == 0){	//in the grandchild process
				
				if(background==1){
							/*if the command is run in background, the child process will block the SIGINT so that
							  the grandchild process running the command will not be affected*/		  
					sigset_t new;
					sigemptyset(&new);
					sigaddset(&new, SIGINT);
					sigprocmask(SIG_BLOCK, &new, NULL);
					}
					
				if(command_counter==0){	//if pipe is not needed
					if(execvp(commands[command_counter][0],commands[command_counter]) == -1){
						printf("myshell: '%s': %s\n", commands[command_counter][0], strerror(errno));
						isTimeX=0;
						exit(-1);
					}
				}else{	//if pipe is needed
				
					int j;
					int pfd[4][2]; //ready 4 pipes
					
					for (j=0;j<=command_counter;j++)
						pipe(pfd[j]);

					if (fork() == 0) { //first command

						if (command_counter>1){ //if more than one pipe is needed

							for(j=1; j<command_counter; j++){ 	//close all the other pipes
								close(pfd[j][0]); 
								close(pfd[j][1]);
							}

						}

						close(pfd[0][0]); 
						dup2(pfd[0][1], 1);  
						if (execvp(commands[0][0], commands[0]) == -1) {
							printf("execvp: error no = %s\n", strerror(errno));
							exit(-1);
						}
					}

					if (command_counter>1){ //if the 2nd pipe is needed
						
						if (fork() == 0) {	
							for(i=0; i<command_counter; i++){ 	//close all the other pipes
								for(j=0; j<2; j++){
									if(!((i==0 && j==0) || (i==1 && j==1)))
										close(pfd[i][j]); 
								}
							}

							dup2(pfd[0][0], 0);  
							dup2(pfd[1][1], 1);  
							if (execvp(commands[1][0], commands[1]) == -1) {
								printf("execvp: error no = %s\n", strerror(errno));
								exit(-1);
							}
						}
				
					}

					if (command_counter>2){ //if the 3rd pipe is needed
						
						if (fork() == 0) {

							for(i=0; i<command_counter; i++){ //close all the other pipes
								for(j=0; j<2; j++){
									if(!((i==1 && j==0) || (i==2 && j==1)))
										close(pfd[i][j]); 
								}
							}

							dup2(pfd[1][0], 0);
							dup2(pfd[2][1], 1);  
							if (execvp(commands[2][0], commands[2]) == -1) {
								printf("execvp: error no = %s\n", strerror(errno));
								exit(-1);
							}
						}

					}
					
					if (command_counter>3){ //if the 4th pipe is needed

						if (fork() == 0) {

							for(i=0; i<command_counter; i++){ //close all the other pipes
								for(j=0; j<2; j++){
									if(!((i==2 && j==0) || (i==3 && j==1)))
										close(pfd[i][j]); 
								}
							}

							dup2(pfd[2][0], 0);
							dup2(pfd[3][1], 1);  
							if (execvp(commands[2][0], commands[2]) == -1) {
								printf("execvp: error no = %s\n", strerror(errno));
								exit(-1);
							}
						}
					}
					


					if (fork() == 0) { //final command

						if (command_counter>1){ //if more than one pipe is needed
							for(j=0; j<command_counter-1; j++){ //close all the other pipes
								close(pfd[j][0]); 
								close(pfd[j][1]);
							}
						}
						
						close(pfd[command_counter-1][1]); 
						dup2(pfd[command_counter-1][0], 0);  
						if (execvp(commands[command_counter][0], commands[command_counter]) == -1) {
							printf("execvp: error no = %s\n", strerror(errno));
							exit(-1);
						}
					}
					int k;
					for(k=0; k<command_counter; k++){ //close all pipes
						close(pfd[k][0]); 
						close(pfd[k][1]);
					}

					for (i=0; i<=command_counter; i++)
						wait(NULL);
					
					return 0;
				}
			}
			
			if(background) {	//if a process is run in background, then set a SIGCHLD handler
				struct sigaction chld_sa;
				sigaction(SIGCHLD, NULL, &chld_sa);
				chld_sa.sa_flags = SA_SIGINFO;
				chld_sa.sa_sigaction = sigchld_handler;
				sigaction(SIGCHLD, &chld_sa, NULL);
			}
			
			long startTime;
			double endTime;

			do{	//child process keep checking the stat of the grandchild process until it becomes a zombie process
				sprintf(str, "/proc/%d/stat", background_id);
				file = fopen(str, "r");
				if (file == NULL) {
					printf("Error in open my proc file\n");
					exit(0);
				}
			fscanf(file, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &sigchldProcessID, str, &str[0], &z, &z, &z, &z, &z,
				(unsigned *)&z, &h, &h, &h, &h, &ut, &st, &h, &h, &h, &h, &h, &h, &h);

				if(str[0]!='Z' && startTime != h)	//get the starting time of the process
					startTime = h;

				fclose(file);

			}while(str[0]!='Z');

			waitpid(background_id, NULL, 0);		// wait for grandchild's termination
			
			//get the time of process termination
			
			file = fopen("/proc/uptime", "r");	
			if (file == NULL) {
				printf("Error in open my proc file\n");
				exit(0);
			}
			fscanf(file, "%lf", &endTime);
			
			if(isTimeX){ 		//if the informations of the process have to be printed 
				printf("PID	CMD		RTIME		UTIME		STIME\n%i	%s		%.2lf s 		%.2lf s		%.2lf s\n", (int) background_id, commands[command_counter][0],endTime-(startTime*1.0f/sysconf(_SC_CLK_TCK)), ut*1.0f/sysconf(_SC_CLK_TCK), st*1.0f/sysconf(_SC_CLK_TCK) );
				isTimeX = 0;
			}
			
			startTime = 0; endTime = 0;		//reset the time variables

			return 0;
		}
		
		if(background==0){	//if the process is not run in background, then the parent process will wait for the child process
			waitpid(process_id,NULL, 0);
		}	

		sigaction(SIGINT, &sa, NULL);	//reset sigaction
		
		for(i=0;i<5;i++)
			free(commands[i]);
	}
	
return 0;

}




