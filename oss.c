#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "child.h"
#include "mlfq.h"
#include "oss.h"


static struct MLFQ mlfq;

static unsigned int child_count = 0;
static unsigned int child_max = 100;
static unsigned int child_done = 0;  //child_done children

static struct oss_time t_idle, t_wait, t_sleep;
static struct oss_time idle_since; /* time since when we are in idle mode */

static struct DATA *data = NULL;
static int qid = -1, shmid = -1;
static const char * logfile = "output.txt";

static int is_signaled = 0; /* 1 if we were signaled */
static int is_idle = 0; /* 1 if we are in idle mode */

static int attach(){

 	key_t k = ftok("oss.c", 1);

 	shmid = shmget(k, sizeof(struct DATA), IPC_CREAT | S_IRWXU);
 	if (shmid == -1) {
 		perror("shmget");
 		return -1;
 	}

 	data = (struct DATA*) shmat(shmid, (void *)0, 0);
 	if (data == (void *)-1) {
 		perror("shmat");
 		shmctl(shmid, IPC_RMID, NULL);
 		return -1;
 	}

	k = ftok("oss.c", 2);
	qid = msgget(k, IPC_CREAT | S_IRWXU);
	if(qid == -1){
		perror("msgget");
		return -1;
	}

	return 0;
}

static int detach(){

  shmdt(data);
  shmctl(shmid, IPC_RMID,NULL);
	msgctl(qid, IPC_RMID, NULL);

  return 0;
}

static void begin_idle(){
  if(is_idle == 0){
    printf("[%li.%li] OSS: No process to dispatch\n", data->clock.sec, data->clock.nsec);
    idle_since = data->clock;
    is_idle = 1;
  }
}

static void end_idle(){
  if(is_idle){	//if simulation was idle

    struct oss_time temp;
    oss_time_substract(&temp, &data->clock, &idle_since);
    oss_time_update(&t_idle, &temp);

    bzero(&idle_since, sizeof(struct oss_time));
    is_idle = 0;
  }
}

static void time_jump(){
  if(q_len(&mlfq.blocked) == 0){
    return;
  }
  const int ci = q_front(&mlfq.blocked);
  struct child * pe = &data->children[ci];

  data->clock = pe->times[T_INOUT];
  printf("[%li.%li] OSS: Time jump\n", data->clock.sec, data->clock.nsec);
}

static int check_blocked(const struct child * children){

  const int ci = mlfq_blocked(&mlfq, &data->clock, children);
  if(ci == -1){
    return -1;
  }
  struct child * pe = &data->children[ci];

  // update total sleep time
  oss_time_update(&t_sleep, &pe->times[T_BURST]);  // burst holds how long user was BLOCKED

  //clear burst and IO times
	bzero(&pe->times[T_INOUT], sizeof(struct oss_time));
	bzero(&pe->times[T_BURST], sizeof(struct oss_time));

  // process becomes READY
  pe->state = CHILD_READY;
  pe->times[T_READY] = data->clock;

  if(q_push(&mlfq.ready[0], ci) < 0){
    fprintf(stderr, "[%li.%li] Error: Queueing blocked process with PID %d failed\n", data->clock.sec, data->clock.nsec, pe->pid);
		return -1;
  }else{
    printf("[%li.%li] OSS: Process with PID %d unblocked to queue 0\n", data->clock.sec, data->clock.nsec, pe->cid);
		return 0;
  }
}

static int dispatch_q(int q){
  struct oss_time t;

  // see whos turn it is to execute
  const int ci = q_front(&mlfq.ready[q]);
  struct child * pe = &data->children[ci];

  printf("[%li.%li] OSS: Dispatching process with PID %u from queue %i\n", data->clock.sec, data->clock.nsec, pe->cid, q);

  // inform process its his time to run
  struct msgbuf mb;
	bzero(&mb, sizeof(struct msgbuf));
  mb.mtype = pe->pid;
  mb.msg = mlfq.quantum[q];

  if( (msgsnd(qid, (void*)&mb, MSG_LEN, 0) == -1) ||
      (msgrcv(qid, (void*)&mb, MSG_LEN, getpid(), 0) == -1)){
    perror("msg");
    return -1;
  }

  // process has executed, remove it from ready queue
  q_pop(&mlfq.ready[q]);

  //upate process burst time and state
  pe->times[T_BURST] = mb.burst;
  pe->state = mb.msg;

	if(pe->state == CHILD_READY){
    printf("[%li.%li] OSS: Process with PID %u burst for %li nanoseconds\n",
      data->clock.sec, data->clock.nsec, pe->cid, pe->times[T_BURST].nsec);

    //update CPU time and move clock forward with burst time
    oss_time_update(&pe->times[T_CPU], &pe->times[T_BURST]);
    oss_time_update(&data->clock, &pe->times[T_BURST]);

    //if process used whole timeslice, it was given
    if(pe->times[T_BURST].nsec == mlfq.quantum[q]){
      //check if we can move to next queue
      if(q < (MLFQ_LEVELS - 1)){
        q++;  //up to next queue level
      }
    }else{
      printf("[%li.%li] OSS: not using its entire time quantum\n", data->clock.sec, data->clock.nsec);
    }
    //save time, since when process becomes ready
    pe->times[T_READY] = data->clock;

    printf("[%li.%li] OSS: Process with PID %u enqueued in level %d\n", data->clock.sec, data->clock.nsec, pe->cid, q);
		q_push(&mlfq.ready[q], ci);

	}else if(pe->state == CHILD_WAIT){


    printf("[%li.%li] OSS: Process with PID %u doing IO until %li.%li\n",
        data->clock.sec, data->clock.nsec, pe->cid, pe->times[T_INOUT].sec, pe->times[T_INOUT].nsec);

    // calculate since when to wait for IO
		oss_time_update(&pe->times[T_INOUT], &data->clock);
  	oss_time_update(&pe->times[T_INOUT], &pe->times[T_BURST]);

    printf("[%li.%li] OSS: Process with PID %u in blocked queue\n", data->clock.sec, data->clock.nsec, pe->cid);
    q_push(&mlfq.blocked, ci);

	}else if(pe->state == CHILD_DONE){
    // process has exited
    child_done++;

    oss_time_update(&pe->times[T_CPU], &pe->times[T_BURST]);
    oss_time_substract(&pe->times[T_SYSTEM], &data->clock, &pe->times[T_START]);
    printf("[%li.%li] OSS: Process with PID %u exited.\n", data->clock.sec, data->clock.nsec, pe->cid);

    // update global wait time
    oss_time_substract(&t, &pe->times[T_SYSTEM], &pe->times[T_CPU]);
    oss_time_update(&t_wait, &t);

    printf("[%li.%li] OSS: Process with PID %u done, dropped from queue %d\n", data->clock.sec, data->clock.nsec, pe->cid, q);
    child_reset(data->children, ci);
  }

  //calculate how long dispatch took

  t.sec = 0;
  t.nsec = rand() % 100;
  printf("[%li.%li] OSS: Time this dispatch took %li nanoseconds\n", data->clock.sec, data->clock.nsec, t.nsec);
  oss_time_update(&data->clock, &t);

  return 0;
}

static int check_ready(){

  //find queue which has process to run
  const int q = mlfq_ready(&mlfq, data->children);
  if(q >= 0){

    end_idle();

    if(dispatch_q(q) < 0){
      return -1;
    }

    mlfq_age(&mlfq, q, &data->clock);


  }else{
    begin_idle();
    time_jump();
  }
  return 0;
}

static void alarm_children(){
  int i;
  struct msgbuf mb;

  bzero(&mb, sizeof(mb));
	mb.msg = -1; //send -1 to make child exit

  for(i=0; i < CHILDREN; i++){
    if(data->children[i].pid > 0){
      mb.mtype = data->children[i].pid;
      if(msgsnd(qid, (void*)&mb, MSG_LEN, 0) == -1){
        perror("msgsnd");
      }
    }
  }
}

static void output_stat(){

  oss_time_divide(&t_wait, child_count);
  oss_time_divide(&t_sleep, child_count);

	printf("Runtime: %li:%li\n", data->clock.sec, data->clock.nsec);
	printf("CPU Idletime: %li:%li\n", t_idle.sec, t_idle.nsec);

  const int runtime = data->clock.sec - t_idle.sec;
  const int util = ( (float)runtime / (float) data->clock.sec) * 100;
  printf("CPU Util: %d %%\n", util);

  // average wait time
  printf("Ave. Wait: %li:%li\n", t_wait.sec, t_wait.nsec);

  // average time a process waited in a blocked queue
  printf("Ave. Wait in Queue : %li:%li\n", t_sleep.sec, t_sleep.nsec);

  printf("Quantum values : ");
	int i;
	for(i=0; i < MLFQ_LEVELS; i++){
		printf("%d ", mlfq.quantum[i]);
	}
	printf("\n");

  //how much proces passed through each queue
  printf("Queue counter : ");
	for(i=0; i < MLFQ_LEVELS; i++){
		printf("%d ", mlfq.ready[i].counter);
	}
	printf("\n");

}


static int user_fork(struct oss_time * fork_time){

  if(child_count >= child_max){  //if we can fork more
    return 0;
  }

  //if its fork time
  if(oss_time_compare(&data->clock, fork_time) == -1){
    return 0;
  }

  //next fork time
  fork_time->sec = data->clock.sec + 1;
  fork_time->nsec = 0;

  const int ci = child_fork(data->children, child_count, &data->clock);
  if(ci == -1){
    return 0;
  }else if(ci > 0){
    if(q_push(&mlfq.ready[0], ci) < 0){
      fprintf(stderr, "[%li.%li] Error: Can't queue process with PID %d\n", data->clock.sec, data->clock.nsec, data->children[ci].cid);
      return -1;
    }else{
      printf("[%li.%li] OSS: Generating process with PID %u in READY queue 0\n", data->clock.sec, data->clock.nsec, data->children[ci].cid);
    }
    child_count++;
  }

  return 1;
}

static int oss_init(){
  t_idle.sec = 0;
  t_idle.nsec = 0;
  t_sleep = t_wait = idle_since =t_idle;

  mlfq_init(&mlfq);

  srand(getpid());

  stdout = freopen(logfile, "w", stdout);
  if(stdout == NULL){
		perror("freopen");
		return -1;
	}
  bzero(data, sizeof(struct DATA));

  return 0;
}

static void move_time(){
  struct oss_time t;		//clock increment
  t.sec = 1;
  t.nsec = rand() % 10000;

  oss_time_update(&data->clock, &t);
}

static void signal_handler(const int signal){
  is_signaled = 1;
  printf("[%li.%li] OSS: Signaled with %d\n", data->clock.sec, data->clock.nsec, signal);
}

int main(void){

  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGALRM, signal_handler);
  signal(SIGCHLD, SIG_IGN);

  if((attach(1) == -1) || (oss_init() == -1)){
    return -1;
  }

  struct oss_time fork_time;	//when to fork another process
	bzero(&fork_time, sizeof(struct oss_time));

  while(!is_signaled && (child_done < child_max)){

    move_time();

		//if we are ready to fork, start a process
    user_fork(&fork_time);

		//if a process is ready with IO, return it to schedule queue
    check_blocked(data->children);
    check_ready();
  }

	output_stat();

  alarm_children();
  detach();
	mlfq_deinit(&mlfq);

  return 0;
}
