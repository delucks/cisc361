#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

/* Jamie Luck
 * jluck@udel.edu * --------------
 * CISC361 - Operating Systems
 * Project 1 - Shell
 * Started 09/16/2014
 * Finished ???
 * Note to grader: I'll be finishing this soon. This is the first submission, which isn't complete. Thanks compilers.
 */

/* section: TODO
 * -------------
 * 1 - Allow no whitespace around | > <
 */

/* section: definitions
 * --------------------
 * Generic type and global var declarations
 */

//#define DEBUG
#define MAXWORDS 100
#define MAXLINELEN 255

#ifdef DEBUG
# define DPRINT(x) printf x
#else
# define DPRINT(x) do {} while(0)
#endif

#ifndef CMD_CHUNK
/* struct: cmd_chunk
 * -----------------
 * Holds metadata information about each atomic chunk of commands
 * between pipes. Handles I/O redirection with file descriptors.
 * Is part of a linked list, starting with head_chunk in main()
 * Traversing the linked list of cmd_chunks is the way to parse
 * I/O redirection
 */
typedef struct cmd_chunk
{
	char * in_path;
	char * out_path;
	char * cmd_raw[MAXWORDS];
	char * cmd_exec[MAXWORDS];
	unsigned int bg;
	int cmd_raw_len;
	int cmd_exec_len;
} cmd_chunk;
#define CMD_CHUNK cmd_chunk
#endif

char PROMPT[] = "dlksh% ";

/* section: functions
 * ------------------
 * Heavy lifting.
 */

int execute(struct cmd_chunk **chunks,char * original,int cmd_chunk_count)
{
	DPRINT(("[::] Attempting to execute %s\n",original));
	// Test for builtins
	if (strcmp(chunks[0]->cmd_exec[0],"exit")==0)
	{
		exit(0);
	}
	else if (strcmp(chunks[0]->cmd_exec[0],"foo")==0)
	{
		// Here she blows! An example builtin
		printf("Bar! :D\n");
	}
	// If we didn't hit any builtins, try to execute it
	else
	{
		int pipe_flag=0,in_fd,out_fd,pipe_fd[2];
		if (cmd_chunk_count > 1)
		{
			// Piped command
			if (strlen(chunks[0]->in_path)>0)
			{
				// We have an input path for the first chunk
				in_fd = open(chunks[0]->in_path,O_RDONLY,0);
				dup2(in_fd,STDIN_FILENO);
				DPRINT(("File input on %u\n",in_fd));
			}
			if (strlen(chunks[1]->out_path)>0)
			{
				// We have an output path for the second chunk
				out_fd = open(chunks[0]->out_path,O_CREAT|O_RDWR,0744);
				dup2(out_fd,STDOUT_FILENO);
				DPRINT(("File output on %u\n",out_fd));
				close(out_fd);
			}
			pipe_flag=1;
		}
		else
		{
			// Single command
			if (strlen(chunks[0]->in_path)>0)
			{
				in_fd = open(chunks[0]->in_path,O_RDONLY,0);
				dup2(in_fd,STDIN_FILENO);
				DPRINT(("File input on %u\n",in_fd));
				close(in_fd);
			}
			if (strlen(chunks[0]->out_path)>0)
			{
				out_fd = open(chunks[0]->out_path,O_CREAT|O_RDWR,0744);
				dup2(out_fd,STDOUT_FILENO);
				DPRINT(("File output on %u\n",out_fd));
				close(out_fd);
			}
		}
		DPRINT(("[exec] Done parsing I/O\n"));
		//printf("size: %u\n",sizeof(chunks[0]->in_path));
		int saved_stdout,saved_stdin;
		if (pipe_flag==1)
		{
			// Set up pipe things
			pipe(pipe_fd);
			saved_stdout = dup(STDOUT_FILENO);
			saved_stdin = dup(STDIN_FILENO);
		}
		int pid = fork();
		if (pid == 0)
		{
			// First child
			DPRINT(("Executing %s\n",chunks[0]->cmd_exec[0]));
			if (pipe_flag==1)
			{
				DPRINT(("Stdout redirected on pipe %u\n",pipe_fd[1]));
				close(pipe_fd[0]);
				dup2(pipe_fd[1],STDOUT_FILENO);
				close(pipe_fd[1]);
				DPRINT(("Stdout duplicated on pipe %u\n",pipe_fd[1]));
			}
			int rc = execvp(chunks[0]->cmd_exec[0], chunks[0]->cmd_exec);
			if (pipe_flag==1)
			{
				// Restore stdout
				//dup2(saved_stdout,STDOUT_FILENO);
				close(pipe_fd[1]);
			}
			DPRINT(("[::] Return code: %i\n",rc));
			if (rc < 0)
			{
				printf("[ERROR] Invalid command entered: %s\n",original);
			}
		}
		else
		{
			if (pipe_flag==1)
			{
				DPRINT(("Setting pipe in on second exec\n"));
				int pid2 = fork();
				if (pid2==0)
				{
					DPRINT(("Stdin redirected on pipe %u\n",pipe_fd[0]));
					close(pipe_fd[1]);
					dup2(pipe_fd[0],STDIN_FILENO);
					close(pipe_fd[0]);
					DPRINT(("Stdin duplicated on pipe %u\n",pipe_fd[0]));
					int rc = execvp(chunks[1]->cmd_exec[0], chunks[1]->cmd_exec);
					DPRINT(("[::] Return code: %i\n",rc));
					if (rc < 0)
					{
						printf("[ERROR] Invalid command entered: %s\n",original);
					}
				}
				else
				{
					//waitpid(pid2,0,0);
				}
			}
			waitpid(pid,0,0);
		}
		close(in_fd);
		close(out_fd);
		close(pipe_fd[0]);
		close(pipe_fd[1]);
	}
	return 0;
}

// changes i/o filepaths into standard input and output designators for pipes
void parse_pipe_io(struct cmd_chunk **input,int chunk_count)
{
	DPRINT(("[::] Parsing pipes\n"));
	unsigned int i=0;
	for (i;i<chunk_count;i++)
	{
		if (input[i]->out_path == NULL)
		{
			input[i]->out_path = "";
		}
		if (input[i]->in_path == NULL)
		{
			input[i]->in_path = "";
		}
		if (i==0)
		{
			//DPRINT(("Setting chunk 0 out_path to stdout\n"));
			// We'll never have stdin as a pipe on the first chunk
			if (strlen(input[i]->out_path)>0)
			{
				// Can't do that! ERROR
				printf("[ERROR] Problem setting stdout of chunk %u\n",i);
			}
			//else
			//{
			//	input[i]->out_path = "stdout";
			//}
		}
		else if (i==(chunk_count-1))
		{
			//DPRINT(("Setting chunk last %u in_path to stdin\n",i));
			// We'll never be setting stdout to a pipe on the last one
			if (strlen(input[i]->in_path)>0)
			{
				//Error
				printf("[ERROR] Problem setting input of chunk %u with length %u\n",i,strlen(input[i]->in_path));
			}
			//else
			//{
			//	input[i]->in_path = "stdin";
			//}
		}
		else
		{
			DPRINT(("Setting chunk %u in and out paths\n",i));
			if (strlen(input[i]->out_path)>0)
			{
				//Error
				printf("[ERROR] Problem setting stdout of chunk %u\n",i);
			}
			else
			{
				//input[i]->out_path = "stdout";
			}
			if (strlen(input[i]->in_path)>0)
			{
				//Error
				printf("[ERROR] Problem setting input of chunk %u\n",i);
			}
			else
			{
				//input[i]->in_path = "stdin";
			}
		}
	}
}

// sets up i/o filepaths if we're doing redirection, sanitizes command of redirection before execution
void parse_chunk_io(struct cmd_chunk *input)
{
	DPRINT(("[::] parse_chunk_io running\n"));
	char * sanitized[MAXWORDS];
	unsigned int iter = 0,san_iter = 0;
	input->out_path = "";
	input->in_path = "";
	for (iter;iter<input->cmd_raw_len;)
	{
		DPRINT(("%s\n",input->cmd_raw[iter]));
		if (strcmp(input->cmd_raw[iter],">") == 0)
		{
			// We've got output redirection into a file
			DPRINT(("out_path set to %s\n",input->cmd_raw[iter+1]));
			input->out_path = input->cmd_raw[iter+1];
			iter = iter+2; //Iterate to skip past these items
		}
		else if (strcmp(input->cmd_raw[iter],"<") == 0)
		{
			// Input redirection from a file
			DPRINT(("in_path set to %s\n",input->cmd_raw[iter+1]));
			input->in_path = input->cmd_raw[iter+1];
			iter = iter+2; //Iterate to skip past these items
		}
		else
		{
			// Normal token, copy it to final array
			sanitized[san_iter] = input->cmd_raw[iter];
			san_iter++;
			iter++;
		}
	}
	// Copy resulting sanitized array into the struct
	memcpy(input->cmd_exec,sanitized,sizeof(sanitized));
	input->cmd_exec_len = iter;
	DPRINT(("[::] Done parsing IO, sanitized len for %s is %u\n",input->cmd_exec[0],san_iter));
}

/* section: main
 * -------------
 * Core program logic
 */

int main(int argc,char** argv)
{
	while (1)
	{
		// Print prompt and read commands
		printf("%s",PROMPT);
		char input[MAXLINELEN];
		fgets(input,MAXLINELEN,stdin);
		char * original = strdup(input);
		DPRINT(("[::] You entered: %s\n",input));
		//TODO 1
		char *tok_curr = strtok(input," \n");
		DPRINT(("[::] Parsing input string.\n"));
		char * arguments[MAXLINELEN];
		unsigned int i = 0,chunkcount=1;
		while (tok_curr != NULL)
		{
			arguments[i] = tok_curr;
			if (strcmp(tok_curr,"|")==0)
			{
				chunkcount++;
			}
			DPRINT(("[::] Argument %i: %s\n",i,tok_curr));
			tok_curr = strtok(NULL," \n");
			i++;
		}
		DPRINT(("[::] Chunking input string into execution blocks\n"));
		struct cmd_chunk * chunks[chunkcount];
		unsigned int j=0;
		unsigned int perchunk=0, inc = 0;
		// Create the chunks array, fill it with command blocks from the parser
		for (j;j<chunkcount;)
		{
			char * foo[MAXLINELEN];
			while (inc<(i))
			{
				// Execute only when we see a pipe or we're at the end of the input tokens
				if (strcmp(arguments[inc],"|")==0)
				{
					inc++;
					DPRINT(("[scanner::round%u] %s is in foo[1]\n",j,foo[1]));
					// Store foo into chunks[j], then inrement j so we hit the for again
					chunks[j] = (struct cmd_chunk*)malloc(sizeof(struct cmd_chunk)+(1*sizeof(foo)));
					memcpy(chunks[j]->cmd_raw,foo,sizeof(foo));
					chunks[j]->cmd_raw_len = perchunk;
					perchunk=0;
					DPRINT(("[scanner::round%u] %s is in chunks[%u][1]\n",j,chunks[j]->cmd_raw[1],j));
					j++;
					// Reset foo to all null values
					memset(&foo[0], 0, sizeof(foo));
					continue;
				}
				DPRINT(("%s\n",arguments[inc]));
				foo[perchunk] = malloc(sizeof(arguments[inc]));
				strncpy(foo[perchunk],arguments[inc],sizeof(arguments[inc])/sizeof(char));
				inc++;
				perchunk++;				
			}
			DPRINT(("[scanner::last] %s is in foo[1]\n",foo[1]));
			// Store foo into the final element of chunks
			chunks[j] = (struct cmd_chunk*)malloc(sizeof(struct cmd_chunk)+(1*sizeof(foo)));
			memcpy(chunks[j]->cmd_raw,foo,sizeof(foo));
			chunks[j]->cmd_raw_len = perchunk;
			perchunk=0;
			DPRINT(("[scanner::last] %s is in chunks[%u][1]\n",chunks[j]->cmd_raw[1],j));
			// Reset foo to all null values
			memset(&foo[0], 0, sizeof(foo));
			j++;
		}
		unsigned int k=0;
		for (k;k<chunkcount;k++)
		{
			DPRINT(("%s is %u bytes. Calling parse_chunk_io\n",chunks[k]->cmd_raw[0],sizeof(chunks[k]->cmd_raw)));
			parse_chunk_io(chunks[k]);
		}
		// if we've got more than one command chunk, set up stdin and out designators for pipes
		if (chunkcount > 1)
		{
			parse_pipe_io(chunks,chunkcount);
		}
		//k=0;
		//int prevoutfd = STDOUT_FILENO;
		//for (k;k<chunkcount;k++)
		//{
		int temp = execute(chunks,original,chunkcount);
		DPRINT(("Finished execute. Relooping\n"));
		//	printf("Output on %u\n",prevoutfd);
		//}
	}
}
