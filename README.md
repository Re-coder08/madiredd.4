to compile use make command
to execute use ./oss command and we can see the output in output.txt file

Aging:
To prevent ready queue starvation, we use an age value for each queue.
The age is the times a queue was not dequeue.

  Each time we dequeue from q queue, we make it younger by 1, and make the other
queues older by 1 age.
  When we reach a certain age, in our case 100, we try to move processes from the
old queue to higher level queue.
for version control: 
I used git 
/classes/OS/madiredd/madiredd.4/log
