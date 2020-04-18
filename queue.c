#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

int q_alloc(queue_t * q, const int qsize){


  q->items = (int*) malloc(sizeof(int)*qsize);
  if(q->items == NULL){
    perror("malloc");
    return -1;
  }

  memset(q->items, -1, sizeof(int)*q->len);
  q->size = qsize;
  q->front = q->back = q->len = 0;

  return 0;
}

void q_dealloc(queue_t * q){
  free(q->items);
}

int q_push(queue_t * q, const int item){
  if(q->len >= q->size){
    return -1;
  }

  q->items[q->back++] = item;
  q->back %= q->size;
  q->len++;
  q->counter++;
  return 0;
}

int q_pop(queue_t * q){
  const unsigned int item = q->items[q->front++];
  q->len--;
  q->front %= q->size;

  return item;
}

int q_front(queue_t * q){
  return q->items[q->front];
}

int q_len(const queue_t * q){
  return q->len;
}

int q_merge(queue_t * to, queue_t * from){
  int count = 0;
  while(q_len(from) > 0){  //have items
    const int item = q_front(from);
    if(q_push(to, item) < 0){  //if full
      break;  //stop
    }
    q_pop(from);
    count++;
  }
  return count;
}
