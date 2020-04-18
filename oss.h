#include "oss_time.h"
#include "child.h"

typedef struct DATA {
	struct oss_time clock;
	struct child children[CHILDREN];
} DATA;

struct msgbuf {
	long mtype;
	int msg;
  struct oss_time burst;
};

#define MSG_LEN sizeof(int) + sizeof(struct oss_time)
