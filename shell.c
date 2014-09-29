#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

/* 
 * Jamie Luck
 * jluck@udel.edu
 *
 * CISC361 - Operating Systems
 * Project 1 - Shell
 * Started 09/16/2014
 * Finished ???
 *
 */

//#define DEBUG

#ifdef DEBUG
# define DPRINT(x) printf x
#else
# define DPRINT(x) do {} while(0)
#endif

char PROMPT[] = "dlksh% ";
size_t MAXLINELEN = 255;

int execute(char * argarray[],char * original)
{
	// Test for builtins
	if (strcmp(argarray[0],"exit")==0)
	{
		exit(0);
	}
	else if (strcmp(argarray[0],"foo")==0)
	{
		// Here she blows! An example builtin
		printf("Bar! :D\n");
	}
	// If we didn't hit any builtins, try to execute it
	else
	{
		DPRINT(("[::] Attempting to execute %s\n",argarray[0]));
		int pid = fork();
		if (pid == 0)
		{
			int rc = execvp(argarray[0], argarray);
			DPRINT(("[::] Return code: %i\n",rc));
			if (rc < 0)
			{
				printf("Invalid command entered: %s",original);
			}
		}
		else
		{
			waitpid(pid,0,0);
		}
	}
}

int main(int argc,char** argv)
{
	while (1)
	{
		printf("%s",PROMPT);
		// Read
		char input[MAXLINELEN];
		fgets(input,MAXLINELEN,stdin);
		char * original = strdup(input);
		DPRINT(("[::] You entered: %s\n",input));
		char *tok_curr = strtok(input," \n");
		DPRINT(("[::] Parsing arguments.\n"));
		char * arguments[MAXLINELEN];
		int i = 0;
		while (tok_curr != NULL)
		{
			arguments[i] = tok_curr;
			DPRINT(("[::] Argument %i: %s\n",i,tok_curr));
			tok_curr = strtok(NULL," \n");
			i++;
		}
		execute(arguments,original);
	}
}
