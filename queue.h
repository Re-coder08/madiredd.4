typedef struct circular_queue {
	int *items;
	int front, back;
	int size;
	int len;
	int counter;
} queue_t;

int q_alloc(queue_t * q, const int size);
void q_dealloc(queue_t * q);

int q_push(queue_t * q, const int item);
int q_pop(queue_t * q);

/* Merge queue from in queue to */
int q_merge(queue_t * to, queue_t * from);

int q_front(queue_t * q);
int q_len(const queue_t * q);
