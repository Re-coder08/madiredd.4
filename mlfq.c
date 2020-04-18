#include <stdio.h>
#include <string.h>

#include "mlfq.h"

int mlfq_init(mlfq_t * mlfq){

  int i;
  memset(mlfq->age, 0, sizeof(int)*MLFQ_LEVELS);

  q_alloc(&mlfq->blocked, CHILDREN);

  for(i=0; i < MLFQ_LEVELS; i++){
    q_alloc(&mlfq->ready[i], CHILDREN);
    mlfq->quantum[i] = (i*2)*QUANTUM;
  }
  mlfq->quantum[0] = QUANTUM;
  return 0;
}

int mlfq_deinit(mlfq_t * mlfq){
  int i;
  for(i=0; i < MLFQ_LEVELS; i++){
		q_dealloc(&mlfq->ready[i]);
	}
	q_dealloc(&mlfq->blocked);
  return 0;
}

void mlfq_age(mlfq_t *mlfq, const int q, const struct oss_time * clock){

	int i;

  //increase deque age
	for(i=0; i < MLFQ_LEVELS; i++){
		mlfq->age[i]++;	//udpate its counter
	}
	mlfq->age[q] -= 2; //dequed queue becomes younger

	for(i=1; i < MLFQ_LEVELS; i++){
		if(	(q_len(&mlfq->ready[i]) != 0)	&&	//if not empty
				(mlfq->age[i] > MLFQ_MAX_AGE)){	  //if old

      const int merge_len = q_merge(&mlfq->ready[i-1], &mlfq->ready[i]);
      mlfq->age[i] = 0;

			printf("[%li.%li] OSS: Queue %d was starving. Moved %d items to previous queue\n",
			   clock->sec, clock->nsec, i, merge_len);
		}
	}
}

// find user we can unblocked
int mlfq_blocked(mlfq_t * mq, const struct oss_time * clock, const struct child * children){
  int i;
  for(i=0; i < mq->blocked.len; i++){
    const int u = q_pop(&mq->blocked);
    if(oss_time_compare(clock, &children[u].times[T_INOUT])){	//if our event time is reached
      return u;
    }
    q_push(&mq->blocked, u);
  }
  return -1;
}

//Find which queue has a ready process
int mlfq_ready(mlfq_t * mlfq, const struct child * children){
  int i;
  for(i=0; i < MLFQ_LEVELS; i++){
    if(q_len(&mlfq->ready[i]) > 0){
      const int u = q_front(&mlfq->ready[i]);
      if(children[u].state == CHILD_READY){
        return i;
      }
    }
  }
  return -1;
}
