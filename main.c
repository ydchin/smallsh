/*************************
* Title: smallsh.c
* Author: Yoon-Orn Chin
* Date: 11/04/2020
* Description: This program creates a shell and executes given commands as it would with linux.
*************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

//Global Variables
char* args[512]; //Max 512 Arguments
char inputName[256]; //Stores > or < if appears
char outputName[256]; //Stores > or < if appears
char* cwd; //Current working directory

int foreground = 1; //Checks to see if foreground process or not. On by default.
int background = 0; //Checks to see if background process or not. Off by default.
int childExitMethod = 0; //Exit Method
int fgOnly = 0; //Foreground only, off by default.
int inputCheck = 0; //Checks to see if there is a valid input name
int outputCheck = 0; //Checks to see if there is a valid output name

//Declaration
struct sigaction SIGINT_action;
struct sigaction SIGSTP_action;

/***********************
* Function: parseString
* Parameter: User Input
* Output: Array with given arguments.
* Why use this: This function is used to parse the user's input and store it into an array for easy access.
***********************/
int parseString(char* input) {
	int count = 0; //Size of Array
	char* saveptr; //Save pointer
	char* token = strtok_r(input, " ", &saveptr); //Parse first argument into token

	//Runs while valid token
	while (token != NULL) {
		//If contains <	
		if (strstr(token, "<")) {
			inputCheck = 1;
			token = strtok_r(NULL, " ", &saveptr); //Skip >
			strcpy(inputName, token); //Copies file name
			token = strtok_r(NULL, " ", &saveptr); //Skip file name
		}

		//If contains >
		else if (strstr(token, ">")) {
			outputCheck = 1;
			token = strtok_r(NULL, " ", &saveptr); //Skip < and takes in file name
			strcpy(outputName, token); //Copies file name
			token = strtok_r(NULL, " ", &saveptr); //Skip file name
		}

		//If contains &
		else if (strstr(token, "&")) {
			background = 1; //Turns on background
			foreground = 0; //Turns off foreground
			break; //& should be at the end, therefore break the loop and don't copy into arguments
		}
		//Otherwise...
		else {
			args[count] = token; //Stores into array
			token = strtok_r(NULL, " ", &saveptr); //Parse next argument and so on
			count++; //Increments
		}
	}
	return count; 
}

//Print Array
void printArray(int sizeArray) {
	//printf("Arguments given: ");
	int num = 0;
	if (strstr(args[0], "echo")) {
		num = 1;
	}
	for (int i = num; i < sizeArray; i++) {
		printf("%s ", args[i]);
		fflush(stdout);
	}
	printf("\n");
	fflush(stdout);
}

/***********************
* Function: doubleDollar
* Parameter: Size of the Array
* Output: Every instance of $$ replaced with PID.
* Why use this: This function is used to take every instance of $$ and replace it with the PID.
* Source: https://stackoverflow.com/questions/2015901/inserting-char-string-into-another-char-string
***********************/
void doubleDollar(int sizeArray) {
	for (int i = 0; i < sizeArray; i++) {
		//Copies current command into temp
		char temp[2048] = "";
		strcpy(temp, args[i]);
		
		//If command contains $$, it runs.
		while (strstr(temp, "$$")){

			//Grabs PID and turns into a string (pid)
			int process_id = getpid();
			char* pid = malloc(sizeof(int));
			sprintf(pid, "%d", process_id);

			//Empty string for post concatination 
			char final[2048] = "";

			int size = 0;

			//Finds index where first $ is located 
			for (int j = 0; j < strlen(temp); j++) {
				//If both $'s are located right next to each other
				if (temp[j] == '$' && temp[j + 1] == '$') {
					//Store index of first $
					size = j;
					break;
				}
			}

			//Copies string and pid into final
			strncpy(final, temp, size);	
			strcat(final, pid);
			strcat(final, (temp + size) + 2);
			strcpy(temp, final);
		}

		//Copies final value as argument
		strcpy(args[i], temp);
		//printf("Result: %s\n", args[i]);
	}
	
}

/***********************
* Function: cd
* Parameter: Size of the Array
* Output: Changes the directory according to given argument.
* Why use this: This function is used to change the current directory to a different one depending on the argument.
* Source: https://stackoverflow.com/questions/9493234/chdir-to-home-directory
***********************/
void cd(int sizeArray) {

	//Grabs current working directory
	char cwdBuffer[PATH_MAX + 1];
	char* tempcwd;
	tempcwd = getcwd(cwdBuffer, PATH_MAX + 1);

	//If cwd is empty, assign it tempcwd.
	if (cwd == NULL) {
		cwd = tempcwd;
	}

	//If there is only 1 argument, pressumably "cd". Go to home directory.
	if (sizeArray == 1) {
		chdir(getenv("HOME"));
		tempcwd = getcwd(cwdBuffer, PATH_MAX + 1);
		//printf("%s\n", cwd);
		//fflush(stdout);
	}

	//If "~", go to home directory.
	else if (strstr(args[1], "~")) {
		chdir(getenv("HOME"));
		//printf("%s\n", cwd);
		//fflush(stdout);
	}

	//Otherwise...
	else {
		//Concatinate the current working directory with the argument given and go to that directory. 
		strcat(tempcwd, "/");
		strcat(tempcwd, args[1]);
		chdir(args[1]);

	}
	
}

/***********************
* Function: exitScript
* Output: Closes the program.
* Why use this: This function is used to exit the program. Easy access to exit command.
***********************/
void exitScript() {
	childExitMethod = 0;
	exit(0);
}

/***********************
* Function: status
* Output: Returns how the child was terminated whether by signal or by status.
* Why use this: This function is used to check of the childExitMethod was closed by status or by signal.
***********************/
void status() {
	if (WIFEXITED(childExitMethod)) {
		//If exited by status
		printf("Exit value %d\n", WEXITSTATUS(childExitMethod));
	}
	else {
		//If terminated by signal
		printf("Terminated by signal %d\n", WTERMSIG(childExitMethod));
	}
}

/***********************
* Function: options
* Parameter: Size of the Array
* Why use this: This function is used to shorten the main function. Provides different options given the argument.
***********************/
void options(int sizeArray) {

	//CD
	if (strstr(args[0], "cd")) {
		cd(sizeArray);
	}

	//Exit
	else if (strstr(args[0], "exit")) {
		exitScript();
	}

	//Status
	else if (strstr(args[0], "status")) {
		status();
	}

	//Otherwise...
	else {
		//Other commands
		options2(sizeArray);
	}

}

/***********************
* Function: options2
* Parameter: Size of the Array
* Why use this: This function is used to shorten the options function. Provides different options given the argument.
***********************/
void options2(int sizeArray) {

	//Processes Slides in lecture
	pid_t spawnpid = -5;
	spawnpid = fork();

	switch (spawnpid) {
		case -1:
			perror("Hull Breach!");
			childExitMethod = 1;
			exit(1);
			break;

		case 0:

			//If foreground, no signal handler
			if (foreground == 1) {
				SIGINT_action.sa_handler = SIG_DFL;
				sigaction(SIGINT, &SIGINT_action, NULL);
			}

			if(inputCheck == 1 || outputCheck == 1){
				//If < exists, redirect data into file.
				if (inputCheck == 1) {
					//Open file
					int fileDesc = open(inputName, O_RDONLY);
					if (fileDesc == -1) {
						printf("Can not open %s\n", inputName);
						fflush(stdout);
						childExitMethod = 1;
						exit(1);
					}

					//Dups file
					if (dup2(fileDesc, 0) == -1) {
						perror("Error dup2.");
						childExitMethod = 1;
						exit(1);
					}

					//Close file
					close(fileDesc);
				}

				//If > exists, redirect data into file.
				if (outputCheck == 1) {

					//Open file
					int fileDescOut = open(outputName, O_WRONLY | O_CREAT | O_TRUNC, 0744);
					if (fileDescOut == -1) {
						printf("Can not open %s\n", outputName);
						fflush(stdout);
						childExitMethod = 1;
						exit(1);
					}

					//Dups file
					if (dup2(fileDescOut, 1) == -1) {
						perror("Error dup2");
						childExitMethod = 1;
						exit(1);
					}

					//Close file
					close(fileDescOut);
				}
			}
			else {
				//If background, open up in /dev/null
				if (background == 1) {

					//Open file into /dev/null
					int fileDescBack = open("/dev/null", O_RDONLY);
					if (fileDescBack == -1) {
						perror("Error open");
						childExitMethod = 1;
						exit(1);

					}

					//Dups file
					if (dup2(fileDescBack, 0) == -1) {
						perror("Error dup2");
						childExitMethod = 1;
						exit(1);
					}

					//Check here
					//Close file
					close(fileDescBack);
				}
			}	
			
			//Executes command
			if (execvp(args[0], args) == -1){ 
				perror(args[0]);
				childExitMethod = 1;
				exit(1);	
			}
			break;

		default:		

			//If it is a background process and foreground mode is off 
			if (background == 1 && fgOnly == 0) {
				waitpid(spawnpid, &childExitMethod, WNOHANG); 
				printf("Background pid is %d\n", spawnpid);
				fflush(stdout);
			}

			//Otherwise...
			else {
				waitpid(spawnpid, &childExitMethod, 0);
			}
			
	}

	//Checks to see if file terminated or not.
	while ((spawnpid = waitpid(-1, childExitMethod, WNOHANG)) > 0) {
		printf("Child process %d terminated: ", spawnpid);
		fflush(stdout);
		status(childExitMethod);
	}
	
}

/***********************
* Function: mainProgram
* Why use this: This function is used as the main loop of the entire shell. Continously runs until given the proper argument.
* Source: https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
***********************/
void mainProgram() {
	char userInput[2048]; //Max 2048 Characters

	//Infinite Loop
	while (1) {

		//Colon start
		printf(": ");
		fflush(stdout);

		//Takes user input
		fgets(userInput, 2048, stdin);

		//Get rids of \n in string
		strtok(userInput, "\n");

		//Keeps track of the amount of arguments
		int sizeArray = parseString(userInput);
		
		//Runs until gets valid input
		while (sizeArray == 0) {
			printf(": ");
			fflush(stdout);
			fgets(userInput, 2048, stdin);
			strtok(userInput, "\n");
			sizeArray = parseString(userInput);
		}

		//If # or nothing in array, continue through loop.
		if (strstr(args[0], "#") || sizeArray == 0) {
			continue; //do nothing
		}

		//Replaces every $$ instance with PID
		doubleDollar(sizeArray);

		//printArray(sizeArray);

		//Go to options
		options(sizeArray);

		//Clear memory
		memset(args, 0, sizeof(args));

		//Set input/output back to none.
		outputCheck = 0;
		inputCheck = 0;

		//Set background and foreground back to normal
		background = 0;
		foreground = 1;

		//Clear input names
		for (int i = 0; i < strlen(inputName); i++) {
			inputName[i] = '\0';
			
		}
		//Clear output names
		for (int j = 0; j < strlen(outputName); j++) {
			outputName[j] = '\0';
		}

	}

}

/***********************
* Function: handle_SIGSTP
* Parameter: Signal
* Output: Enters or Exits foreground-only mode.
* Why use this: This function is used to keep track of if ^Z is called. If it is, then it turns on foreground-only mode and off if called again.
***********************/
void handle_SIGSTP(int signal) {
	//If not foreground only mode...
	if (fgOnly == 0) {
		printf("Entered Foreground-only mode\n");
		fflush(stdout);
		fgOnly = 1; //Turns on foreground only mode
	}

	//Otherwise..
	else {
		printf("Exited Foreground-only mode\n");
		fflush(stdout);
		fgOnly = 0; //Turns foreground only mode off
	}
}

/***********************
* Function: signalHandler
* Why use this: This function is used to keep track of if ^C or ^Z are called. If they are, then it'll either be ignored or activate handle_STP.
***********************/
void signalHandler() {

	//Handles signal to check for ^C and ignores it.
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	//Handles signal to check for ^Z.
	SIGSTP_action.sa_handler = handle_SIGSTP;
	sigfillset(&SIGSTP_action.sa_mask);
	SIGSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGSTP_action, NULL);
	
}

//Main function :)
int main() {

	//Handles signals
	signalHandler();

	//Executes main program
	mainProgram();
}