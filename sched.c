#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*           _              _       
 *  ___  ___| |__   ___  __| |  ___ 
 * / __|/ __| '_ \ / _ \/ _` | / __|
 * \__ \ (__| | | |  __/ (_| || (__ 
 * |___/\___|_| |_|\___|\__,_(_)___|
 *                                  
 * OS Class Project
 * Author: Jamie Luck (jluck@udel.edu)
 *
 */

/* =========================================================================================
 * Compiling, Usage, Design Decisions
 *
 Compiling:
 * sparc with solaris cc:
 *   cc -g -o sched -xc99 sched.c
 * x86 with gcc:
 *   cc -g -o sched -std=c99 sched.c
 *
 Usage:
 * ./sched {input.txt} FCFS {quantum} {half-cs cost}
 *
 Design Decisions:
 * In FCFS with pre-emption, I charge a context switch cost when there's only one process to be run on the CPU and its current burst exceeds the quantum
 * Specifying non-preemptive behavior is done by setting the quantum parameter in the command line to 0.
 * I didn't implement SJNFP
 * When compiling on x86, there are some free() calls that may get an error. Since the target platform is SPARC I've left them in. They're marked in the code.
 * They may be fixed eventually.
 *
 */

// =========================================================================================
// Structs

typedef struct process
{
	int* order; // read automatically to be the ints for cpu/io, in alterating order
	int proc_num;
	int start;
	int cpu_req;
	int time_first_cpu;
	int io_wait; // total of i/o burst times
	int cpu_total; // total of cpu burst times
	int time_finish;
	int time_wait;
	int time_response;
	int time_turnaround;
	int return_time; // for use with blocking i/o
} process;

typedef struct running_process
{
	process* current;
	int run_until;
	int signal_cs;
} running_process;

typedef struct QueueNode
{
	process* proc;
	struct QueueNode* next;
} QueueNode;

typedef struct Queue
{
	QueueNode* head;
	QueueNode* tail;
	int size;
} Queue;

// =========================================================================================
// Methods

void usage()
{
	printf("Usage: ./sched {input file} FCFS {quantum} {half-context switch cost}\n       If you would like round-robin behavior, set quantum to 0.\n       Written by Jamie Luck for CISC361.\n\n");
}

Queue* createQueue()
{
	Queue* q = (Queue*)malloc(sizeof(Queue));
	q->head = NULL;
	q->tail = NULL;
	q->size = 0;
	return q;
}

void queue_push(Queue* q,process* proc)
{
	QueueNode* n=(QueueNode*)malloc(sizeof(QueueNode));
	n->proc=proc;
	n->next=NULL;
	if (q->head==NULL)
	{
		q->head=n;
	}
	else
	{
		q->tail->next=n;
	}
	q->tail=n;
	q->size++;
}

process* queue_pop(Queue* q)
{
	if (q->head != NULL)
	{
		QueueNode* oldhead = q->head;
		process* item = oldhead->proc;
		q->head = oldhead->next;
		q->size--;
		free(oldhead);
		return item;
	}
	else
	{
		return NULL;
	}
}

process* queue_peek(Queue* q)
{
	return q->head->proc;
}

int fcfs(char* inputfile,int quantum,int halfcs)
{
	int line_num = 0;
	FILE* tfp = fopen(inputfile,"r");
	if (tfp == NULL)
		exit(EXIT_FAILURE);
	char ch = getc(tfp);
	while (ch != EOF)
	{
		if (ch=='\n')
		{
			line_num++;
		}
		ch = getc(tfp);
	}
	fclose(tfp);
	// line_num now holds the amount of processes we're going to need to schedule
	process* procs[line_num];
	// now reopen the file and actually create the pcbs
	char line[BUFSIZ];
	FILE* fp = fopen(inputfile,"r");
	int proc_itr = 0;
	while (fgets(line,sizeof(line),fp))
	{
		int token_count = 0;
		char* tok_curr = strtok(line," \n");
		int* order;
		procs[proc_itr] = (process*)malloc(sizeof(process));
		while (tok_curr != NULL)
		{
			if (token_count == 0)
			{
				procs[proc_itr]->start = strtol(tok_curr,(char**)NULL,10);
			}
			else if (token_count == 1)
			{
				procs[proc_itr]->cpu_req = strtol(tok_curr,(char**)NULL,10);
				order = (int*)malloc(8*procs[proc_itr]->cpu_req);
			}
			else if ((token_count - 2)%2 == 0)
			{
				// -2 because the first two are the process description and full cpu requirements
				int this_burst = strtol(tok_curr,(char**)NULL,10);
				procs[proc_itr]->cpu_total = procs[proc_itr]->cpu_total + this_burst;
				order[token_count-2] = this_burst;
			}
			else if ((token_count - 2)%2 == 1)
			{
				int this_io = strtol(tok_curr,(char**)NULL,10);
				procs[proc_itr]->io_wait = procs[proc_itr]->io_wait + this_io;
				order[token_count-2] = this_io;
			}
			tok_curr = strtok(NULL," \n");
			token_count++;
		}
		procs[proc_itr]->proc_num = proc_itr;
		procs[proc_itr]->order = order;
		procs[proc_itr]->time_first_cpu = -1;
		proc_itr++;
	}
	fclose(fp);

	// iterate through 
	Queue* ready_queue = createQueue();
	// create block queue and set them all to null
	process* block_queue[line_num];
	for (int i=0;i<line_num;i++)
	{
		block_queue[i] = NULL;
	}
	int time = 0;
	int finished = 0;
	int noprocs = 0;
	running_process* rp = (running_process*)malloc(sizeof(running_process));
	rp->current = NULL;
	rp->signal_cs = 0;
	printf("[time] event description\n========================\n\n");
	while (!finished)
	{
		// check if a new process needs to be added to queue, if so do it and print
		for (int i=0;i<line_num;i++)
		{
			if (procs[i]->start == time)
			{
				queue_push(ready_queue,procs[i]);
				printf("[%d] process %i pushed to ready queue\n",time,procs[i]->proc_num);
			}
			if (block_queue[i] != NULL)
			{
				// check if a blocked process needs to be added back to queue, if so do it and print
				if (block_queue[i]->return_time == time)
				{
					queue_push(ready_queue,block_queue[i]);
					block_queue[i]->return_time = -1;
					printf("[%d] process %i returns from blocked state to ready queue\n",time,block_queue[i]->proc_num);
					block_queue[i] = NULL;
				}
			}
		}
		// Algorithm block
		if (rp->signal_cs > 0) // waiting for a context switch
		{
			rp->signal_cs--; // decrement until c.s. is finished
		}
		else if (rp->current != NULL)		// if there's a process on the CPU and we're not waiting for a context switch
		{
			if (rp->run_until == time) // processed finished running for current time allocation
			{
				process* old = rp->current;
				if (rp->current->order[0] == 1) // yay, we reached the end of its running block
				{
					printf("[%u] process %u has finished its current CPU burst\n",time,rp->current->proc_num);
					rp->current->order++; // increment it to skip past the first pointer
					rp->current->return_time = time + rp->current->order[0]; // set time to return after I/O burst
					if (rp->current->return_time == time)
					{
						rp->current->time_finish = time;
						rp->current->time_turnaround = time - rp->current->time_first_cpu;
						rp->current->time_response = rp->current->time_first_cpu - rp->current->start;
						rp->current->time_wait = rp->current->time_turnaround - rp->current->cpu_total;
						printf("[%u] process %u terminated.\n  TAT: %u\n  Wait time: %u\n  I/O Wait: %u\n  Response time: %u\n",time,rp->current->proc_num,rp->current->time_turnaround,rp->current->time_wait,rp->current->io_wait,rp->current->time_response);
					}
					else
					{
						printf("[%u] process %u waiting for I/O until %u\n",time,rp->current->proc_num,rp->current->return_time);
						for (int i=0;i<line_num;i++)
						{
							if (block_queue[i] == NULL)
							{
								block_queue[i] = old;
								break;
							}
						}
					}
				}
				else // still has some time to go
				{
					queue_push(ready_queue,old);
					printf("[%u] process %u returned to ready queue after quantum expiration\n",time,old->proc_num);
				}
				rp->current = NULL;
				// we need to context switch it off the CPU and start its wait cycle
				// unless there are no more cpu/wait cycles left, then remove it entirely
			}
			else
			{
				// let it continue running
				rp->current->order[0]--;
				//printf("process %u has now %u remaining in its CPU burst\n",rp->current->proc_num,rp->current->order[0]);
			}
		}
		else // there's no process currently on the CPU (rp->current == NULL)
		{
			if (rp->signal_cs==0)
			{
				// perform the context switch. take a job off the ready queue, put it on the cpu and rp, start it running for one full quantum
				process* top = queue_pop(ready_queue);
				char* message;
				if (top != NULL)
				{
					if (quantum == 0) // round robin
					{
						rp->run_until = time + top->order[0];
						message = "end of current burst";
					}
					else // we've got some context switchin' to do
					{
						if (top->order[0] < quantum)
						{
							rp->run_until = time + top->order[0];
							message = "end of current non-preempted burst";
						}
						else
						{
							rp->run_until = time + (quantum-1); // dat process has more cpu time than the current quantum
							message = "running one quantum";
						}
					}
					rp->current = top;
					printf("[%u] process %u on CPU until %u (%s)\n",time,top->proc_num,rp->run_until,message);
					if (rp->current->time_first_cpu == -1)
					{
						rp->current->time_first_cpu = time;
					}
				}
				else
				{
					// try to tell if we're done with execution. we'll be done if there's nothing in the ready queue or in the blocked queue, and no processes haven't been started
					int done=1;
					if (ready_queue->size==0)
					{
						for (int i=0;i<line_num;i++)
						{
							if (block_queue[i] != NULL)
							{
								done = 0; // we have a process waiting in the block_queue structure
							}
							if (procs[i]->start > time)
							{
								done = 0; // a process has yet to be started
							}
						}
					}
					else
					{
						done = 0; // ready queue isn't empty
					}
					if (done)
					{
						finished = 1;
						// Calculate statistics to be reported in the end-of-run report
						double utilization = ((double)(time-noprocs))/(double)time;
						int total_tat,total_wait,total_resp = 0;
						int max_tat,max_wait,max_resp = 0;
						double avg_tat,avg_wait,avg_resp = 0.0;
						for (int i=0;i<line_num;i++)
						{
							if (procs[i]->time_turnaround > max_tat)
							{
								max_tat = procs[i]->time_turnaround;
							}
							if (procs[i]->time_wait > max_wait)
							{
								max_wait = procs[i]->time_wait;
							}
							if (procs[i]->time_response > max_resp)
							{
								max_resp = procs[i]->time_response;
							}
							total_tat += procs[i]->time_turnaround;
							total_wait += procs[i]->time_wait;
							total_resp += procs[i]->time_response;
						}
						avg_tat = (double)total_tat/(double)line_num;
						avg_wait = (double)total_wait/(double)line_num;
						avg_resp = (double)total_resp/(double)line_num;
						printf("\n[%u] Simulation Finished\n  CPU Utilization: %f %%\n  Avg. TAT: %f\n  Max. TAT: %u\n  Avg. Wait Time: %f\n  Max. Wait Time: %u\n  Avg. Response Time: %f\n  Max. Response Time: %u\n\n",time,utilization,avg_tat,max_tat,avg_wait,max_wait,avg_resp,max_resp);
					}
					else
					{
						noprocs++; // nothing runs on the CPU this cycle
					}
				}
				rp->signal_cs = -1;
			}
			else
			{
				rp->signal_cs = halfcs; // nothing is running on the CPU. signal a context switch to be performed
			}
		}
		// increment time
		time++;
	}
	// NOTE: If you're compiling on x86 you may get some invalid free's on these free() calls. I'm leaving them in because target platform is solaris sparc
	for (int i=0;i<line_num;i++)
	{
		free(procs[i]->order);
	}
	free(procs);
	free(ready_queue);
	return 0;
}

// =========================================================================================
// Main

int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		usage();
		exit(EXIT_FAILURE);
	}
	else
	{
		if (strcmp(argv[2],"FCFS")==0)
		{
			if (argc != 5)
			{
				fprintf(stderr,"Invalid number of arguments (%d) to FCFS, 4 expected. See ./sched for usage information\n",argc-1);
				exit(EXIT_FAILURE);
			}
			else
			{
				int quantum = strtol(argv[3],(char**)NULL,10);
				if (quantum != 0)
					quantum++; // prevent starvation
				int halfcs = strtol(argv[4],(char**)NULL,10);
				fcfs(argv[1],quantum,halfcs);
			}
		}
		else if (strcmp(argv[2],"SJNFP")==0)
		{
			if (argc != 4)
			{
				fprintf(stderr,"invalid number of arguments (%d) to SJNFP, 3 expected\n",argc-1);
				exit(EXIT_FAILURE);
			}
			else
			{
				//sjnfp(argv[1],argv[3]);
				fprintf(stderr,"SJNFP not implemented, come back another time :)\n");
			}
		}
		else
		{
			fprintf(stderr,"invalid scheduling algorithm %s\n",argv[2]);
			exit(EXIT_FAILURE);
		}
	}
}
