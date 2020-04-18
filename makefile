CC=gcc
CFLAGS=-Wall -g -pedantic

sim: oss user

oss: oss.c oss_time.o queue.o mlfq.o child.o oss.h
	$(CC) $(CFLAGS) oss.c oss_time.o queue.o mlfq.o child.o -o oss -lrt

user: user.o oss_time.o mlfq.o child.o oss.h
	$(CC) $(CFLAGS) user.o oss_time.o queue.o mlfq.o child.o -o user

user.o: user.c child.h
	$(CC) $(CFLAGS) -c user.c

oss_time.o: oss_time.h oss_time.c
	$(CC) $(CFLAGS) -c oss_time.c

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c queue.c

mlfq.o: mlfq.c mlfq.h
	$(CC) $(CFLAGS) -c mlfq.c

clean:
	rm -f ./oss ./user *.log *.o output.txt
