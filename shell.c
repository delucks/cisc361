#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

// woo a fucking shell

/* Parsing
 *   char *ptr = strtok(input," \n");
 *   printf("strtok: %s\n",ptr);
 * Prints first token from string
 *   char * bar = strtok(NULL," \n");
 *   printf("2: %s\n",bar);
 * Assigns ptr to second token
 *   ptr = strtok(NULL," \n");
 *   printf("3: %s\n",ptr);
 * Et cetera
 */

// Change this to disable debug printing information
//#define DEBUG

#ifdef DEBUG
# define DPRINT(x) printf x
#else
# define DPRINT(x) do {} while(0)
#endif

char PROMPT[] = "delucks > ";
size_t MAXLINELEN = 255;

int main(int argc,char** argv)
{
	char input[MAXLINELEN];
	while (1)
	{
		printf("%s",PROMPT);
		// Read
		fgets(input,MAXLINELEN,stdin);
		char * original = strdup(input);
		DPRINT(("[::] You entered: %s\n",input));
		char *tok_curr = strtok(input," \n");
		DPRINT(("[::] Parsing arguments.\n"));
		char * arguments[MAXLINELEN];
		int i = 0;
		while (tok_curr != NULL) {
			arguments[i] = tok_curr;
			DPRINT(("[::] Argument %i: %s\n",i,tok_curr));
			tok_curr = strtok(NULL," \n");
			i++;
		}
		int pid = fork();
		if (pid == 0) {
			int rc = execvp(arguments[0], arguments);
			DPRINT(("[::] Return code: %i\n",rc));
			if (rc < 0) {
				printf("Invalid command entered: %s",original);
			}
		}
		else {
			waitpid(pid,0,0);
		}
		//tok_first = strtok(NULL," \n");
		//if (tok_first != NULL) {
		//	DPRINT(("2: %s\n",tok_first));
		//}
		//else {
		//	DPRINT(("NULL!"));
		//}
		//rc = execvp(tok_first, arguments);
	}
}
