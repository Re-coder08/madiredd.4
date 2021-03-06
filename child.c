#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "child.h"

#define TOGGLE(map, bit) map ^= (1 << bit);
#define ISSET(map, bit)  (map & (1 << bit)) >> i

static unsigned int user_bitmap = 0;

//check which user entry is free, using bitmap
static int find_free_index(){
	int i;
  for(i=0; i < CHILDREN; i++){
  	if((ISSET(user_bitmap, i)) == 0){
      TOGGLE(user_bitmap, i);
      return i;
    }
  }
  return -1;
}

//Clear a user entry
void child_reset(struct child * children, const unsigned int i){

  TOGGLE(user_bitmap, i);
  bzero(&children[i], sizeof(struct child));
}

int child_fork(struct child * children, const int id, const struct oss_time * t){

  struct child *pe;

  const int ci = find_free_index();
	if(ci == -1){
		return -2;  //no space for a new child
	}

  pe =  &children[ci];

  pe->cid	= id;
  pe->type = ((rand() % 100) > 80) ? CHILD_NORMAL : CHILD_REAL;
  pe->state = CHILD_READY;
	pe->times[T_START] =
  pe->times[T_READY] = *t;


	const pid_t pid = fork();
	switch(pid){
		case -1:
			child_reset(children, ci);
			perror("fork");
      return -1;
			break;

		case 0:

			execl("./user", "./user", NULL);
			perror("execl");
			exit(EXIT_FAILURE);

		default:
			pe->pid = pid;
			break;
  }

  return ci;
}
