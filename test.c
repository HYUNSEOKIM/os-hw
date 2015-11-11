#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>

enum state{
	WAIT,
	READY,
	DONE,
};

//structure declaration
struct PCB{
	pid_t pid;
	enum state state;
	int cpu_b;
	int io_b;
}PCB;

struct proc_node{
	struct PCB *data;
	struct proc_node *next;
}NODE;

struct proc_q{
	struct proc_node *front;
	struct proc_node *back;
	int size;
}QUEUE;

struct msg{
	long mstate;
	pid_t pid;
	int io;
}MSG;

//function declaration
struct proc_node *createnode(struct PCB *p);
void init_q(struct proc_q *q);
void destroy_q(struct proc_q *q);
void enqueue_proc(struct proc_q *q,struct PCB *p);
struct PCB dequeue_proc(struct proc_q *q);
void do_child(int i, pid_t pid);
void time_tick (int signo);
void wait_to_run();
void run_to_wait();

//Global variables
struct PCB *process[10];
int global_tick;
struct PCB *now;
struct proc_q *run_q;
struct proc_q *wait_q;

int main (){
	int i=0;
	global_tick=0;
	pid_t pid;
	int msqid=0;
	int tmp=0;

	struct sigaction act;
	struct itimerval timer;
	struct msg msg_buf;
	
	//initialize both wait_q and run_q
	init_q(wait_q);
	init_q(run_q);

	for(i=0; i<10; i++){
		process[i] = (struct PCB*)malloc(sizeof(PCB));
		memset(&process[i],0,sizeof(PCB));
	}

	//to get random bursts
	srand((unsigned)time(NULL)+(unsigned)getpid());
	
	//Install timer_handler as the signal handler for SIGALARM
	memset(&act,0,sizeof(act));
	act.sa_handler = &time_tick;
	sigaction(SIGALRM,&act,NULL);

	//Cpmfogire the timer to expire after 250 msec
	timer.it_value.tv_sec = 1;
	timer.it_value.tv_usec = 0;

	//...and every 250 msec after that.
	timer.it_interval.tv_sec=1;
	timer.it_interval.tv_usec = 0;

	//Start a timer. It counts down whenever this process is executing.
	global_tick = 0;
	setitimer(TIMER_REAL,&timer,NULL);

	for(i=0; i<10; i++){
		pid = fork();

		//parent
		if(pid > 0){
			enqueue_proc(run_q,process[i]);

			msqid = msgget((key_t)1234,IPC_CREAT | 0644);

			if(msqid == -1) 
				printf("MSG_Q creation error!\n");

			else{
				printf("MSG_Q (key : 0x%x, id : 0x%x) created.\n",1234,msqid);
			}

			while(1){
				if(msqid>0){
					tmp = msgrcv(msqid,&msg_buf,sizeof(struct msg),0,0); //WAIT state

					if(tmp<0)
						printf("msgrcv() fail\n");
					else{
						printf("ID = [%d] MSG = %s\n", msgid,msg_buf.buff);
					}
				}

			}									

		//child
		}else if (pid == 0){

			do_child(i,pid);
			
		}else printf("error\n");

	}
}


struct proc_node *createnode(struct PCB *p){
	struct proc_node *node = (struct proc_node*)malloc(sizeof(struct proc_node));
	if (node == NULL) error("out of memory\n");

	node->data = p;
	node->next = NULL;
	return node;
}

void init_q(struct proc_q *q){
	q = (struct proc_q*)malloc(sizeof(struct proc_q));
	q->front = q->back = NULL;
	q->size = 0;
}

void destroy_q(struct proc_q  *q){
	while(q->front != NULL){
		struct proc_node *tmp = q->front;
		q->front = tmp->next;
		free(tmp);
	}

	free(q);
}
		

void enqueue_proc(struct proc_q *q, struct PCB *p){
	struct proc_node *node = createnode(p);
	if(q->front == NULL){
		q->front = q->back = node;
	}

	else{
		q->back->next = node;
		q->back = node;
	}

	q->size++;
}

struct PCB dequeue_proc(struct proc_q *q){
	struct proc_node *delete = q->front;
	struct PCB tmp_pcb;

	if(q->size == 1){
		q->front = NULL;
		q->back = NULL;
	}else if(q->size > 1){
		if(q->front->next != NULL)		
			q->front = delete->next;
			tmp_pcb=*delete->data;
			free(delete);
	}
	q->size--;
	return tmp_pcb;
}
void time_tick (int signo)
{
	global_tick++;

	if(global_tick>30){
		int i =0;
		for (i=0; i<10; i++)
			kill (process[i]->pid,SIGKILL);

		kill(getpid(),SIGKILL);

		return;
	}
}

void do_child(int i, pid_t pid){

	process[i]->pid = pid;
	process[i]->cpu_b = rand()%100+1;
	process[i]->io_b = rand()%100+1;
	process[i]->state = READY;


}


void wait_to_run(){

	int i;
	int wait_q_size = wait_q->size;
	struct PCB *target;

	if(wait_q_size == 0)
		return;

	for(i=0;i<wait_q_size;i++){
		*target = dequeue_proc(wait_q);
		
		//update target's PCB
		if(target->io_b <= 0 && target->state == WAIT){//goes to run_q
			target->state = READY;
			target->cpu_b = rand()%100+1;
			target->io_b = rand()%100+1;
	
			enqueue_proc(run_q,target);
		}else if(target->io_b <= 0 && target->cpu_b <= 0){//retired process
			target->state = DONE;
		}else if(target->io_b > 0){ //stay in wait_q
			enqueue(wait_q,target);
		}
	}

	return;
}	

void run_to_wait(){

	run_q->front->data.state = WAIT;

	dequeue_proc(run_q);
   	enqueue_proc(wait_q);

	//if cpu_b == 0 : retired process 
	//this process should go to wait_q
	//change the state of it as follosing:  DONE (declared in enum state)

   return;
}

int schedule(){

	struct PCB *next;

	if(run_q != NULL)
		next = run_q->process;
	else{
		if(wait_q  != NULL){
			


