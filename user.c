#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include "oss.h"

static struct DATA * data = NULL;

int attach(){

 key_t k = ftok("oss.c", 1);

 const int shmid = shmget(k, sizeof(struct DATA), 0);
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
 const int qid = msgget(k, 0);
 if(qid == -1){
	 perror("msgget");
	 return -1;
 }

 return qid;
}

int main(void){

	struct msgbuf buf;

	const int qid = attach();
	if(qid < 0){
		return -1;
	}

	srand(getpid());

	buf.msg = CHILD_READY;

  while(buf.msg != CHILD_DONE){

		bzero(&buf, sizeof(struct msgbuf));
		
		if(msgrcv(qid, (void*)&buf, MSG_LEN, getpid(), 0) == -1){
	    perror("msgrcv");
	    break;
	  }

		const int timeslice = buf.msg;
		if(timeslice < 0){
			break;
		}

		//what decision the user will take.
		const int decision = ((rand() % 100) < 15) ? 4 : (rand() % 3);

		if(decision == 0){	// take whole timeslice

			buf.msg = CHILD_READY;
			buf.burst.sec = 0;
			buf.burst.nsec = timeslice;

		}else if(decision == 1){	// input/output delay

			buf.msg = CHILD_WAIT;
			buf.burst.sec  = rand() % 5;
			buf.burst.nsec = rand() % 1000;
		}else if(decision == 2){ //part of timeslice

			buf.msg = CHILD_READY;
			buf.burst.sec = 0;
			buf.burst.nsec = ((float) timeslice / (100.0f / (1.0f + (rand() % 99))));

		}else{	// we terminate
			buf.msg = CHILD_DONE;
			// use [1;99] of timeslice
			buf.burst.sec = 0;
			buf.burst.nsec = ((float) timeslice / (100.0 / (1.0 + (rand() % 99))));
		}

		buf.mtype = getppid();
		if(msgsnd(qid, (void*)&buf, MSG_LEN, 0) == -1){
	    perror("msgsnd");
	    break;
	  }
  }

	shmdt(data);

  return 0;
}
