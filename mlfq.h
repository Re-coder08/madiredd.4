#include "oss_time.h"
#include "child.h"
#include "queue.h"

// quantum ( 10 ms )
#define QUANTUM 10000000

//number of schedule queues
#define MLFQ_LEVELS 4

//If a queue doesn't get dequeued in 50 dispatches, its starving
#define MLFQ_MAX_AGE 100

//Multi-Level Feedback Queue
typedef struct MLFQ {
  //Queues
  queue_t blocked;
  queue_t ready[MLFQ_LEVELS];

  //quantum for each queue
  int quantum[MLFQ_LEVELS];

  //dequeue age of each queue
  int age[MLFQ_LEVELS];

} mlfq_t;

int mlfq_init(mlfq_t * mlfq);
int mlfq_deinit(mlfq_t * mlfq);

int mlfq_blocked(mlfq_t * mlfq, const struct oss_time * clock, const struct child * children);
int mlfq_ready(mlfq_t * mlfq, const struct child * children);

void mlfq_age(mlfq_t *mlfq, const int q, const struct oss_time * clock);
