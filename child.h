#ifndef CHILD_H
#define CHILD_H

#include "oss_time.h"
#include <sys/types.h>

#define CHILDREN 20

enum state { CHILD_READY=1, CHILD_WAIT, CHILD_DONE };
enum type  { CHILD_NORMAL=1, CHILD_REAL};
enum times { T_START=0, T_READY, T_BURST, T_CPU, T_SYSTEM, T_INOUT };

struct child {
	pid_t	pid;	/* process ID */
	int cid;		/* child   ID */

	enum type	 type;	/* normal or real-time */
	enum state state;	/* process state */

	struct oss_time	times[6];
};

void child_reset(struct child * children, const unsigned int i);
int child_fork(struct child * children, const int id, const struct oss_time * t);

#endif
