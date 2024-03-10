#ifndef __SIMPLE_QUEUE__
#define __SIMPLE_QUEUE__

typedef struct Queue *Queue;

struct Queue* newQueue(int capacity);

int enqueue(struct Queue* q, int value, struct timeval arrival_time);

int dequeue(struct Queue* q);
int dequeue_by_fd(struct Queue* q, int value);
int dequeue_by_index(struct Queue* q, int index);

int queue_empty(struct Queue* q);
int get_queue_size(struct Queue* q);

struct timeval get_head_arrival(struct Queue* q);

void freeQueue(struct Queue* q);
#endif