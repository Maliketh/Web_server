#include <stdlib.h>
#include "Queue.h"
#include <pthread.h>

static struct Node {
    int value;
    struct timeval arrival_time;

    struct Node* next;
};

typedef struct Queue {
    int size;
    int max_size;

    struct Node* head;
    struct Node* tail;

    //cond_t c; // should be initialized
    //mutex_t m; // should be initialized

};

struct Queue* newQueue(int capacity)
{
    struct Queue* q;
    q = malloc(sizeof(struct Queue));

    if (q == NULL) {
        return q;
    }

    q->size = 0;
    q->max_size = capacity;
    q->head = NULL;
    q->tail = NULL;

    return q;
}

int enqueue(struct Queue* q, int value, struct timeval arrival_time)
{
    if ((q->size + 1) > q->max_size) {
        return q->size;
    }
    //mutex_lock(&m);
    struct Node* node = malloc(sizeof(struct Node));

    if (node == NULL) {
        return q->size;
    }

    node->value = value;
    node->arrival_time = arrival_time;
    node->next = NULL;

    if (q->head == NULL) {
        q->head = node;
        q->tail = node;
        q->size = 1;

        return q->size;
    }


    q->tail->next = node;
    q->tail = node;
    q->size += 1;

    //cond_signal(&c);
    //mutex_unlock(&m);

    return q->size;
}

int dequeue(struct Queue* q)
{
    /*
    mutex_lock(&m);
    while (q->size == 0) {
        cond_wait(&c, &m);
    }
    */


    int value = 0;
    struct Node* tmp = NULL;

    value = q->head->value;
    tmp = q->head;
    q->head = q->head->next;
    q->size -= 1;
    //mutex_unlock(&m);

    if(q->size == 0)
        q->tail = NULL;

    free(tmp);

    return value;
}

int dequeue_by_fd(struct Queue* q, int fd)
{
    /*
    mutex_lock(&m);
    while (q->size == 0) {
        cond_wait(&c, &m);
    }
    */

    struct Node* current = (q->head)->next;
    struct Node* prev = q->head;

    if(q->head->value == fd){
        q->head = (q->head)->next;
        q->size -= 1;

        if(q->size == 0)
            q->tail = NULL;

        //mutex_unlock(&m);
        free(prev);
        return fd;
    }

    while(current->value != fd && current != NULL){
        current = current->next;
        prev = prev->next;
    }

    if(current){
        prev->next = current->next;
        q->size -= 1;

        if(q->size == 0)
            q->tail = NULL;

        if(current == q->tail)
            q->tail = prev;
        //mutex_unlock(&m);
        free(current);
        return fd;
    }

    return -1;
}

int dequeue_by_index(struct Queue* q, int index){
    struct Node* current = (q->head)->next;
    struct Node* prev = q->head;
    int current_index = 0;
    int fd = current->value;

    if(index == 0){
        fd = prev->value;
        q->head = (q->head)->next;
        q->size -= 1;

        if(q->size == 0)
            q->tail = NULL;

        //mutex_unlock(&m);
        free(prev);
        return fd;
    }

    while(index != current_index && current != NULL){
        current = current->next;
        prev = prev->next;
        current_index++;
    }

    if(current){
        fd = current->value;
        prev->next = current->next;
        q->size -= 1;

        if(q->size == 0)
            q->tail = NULL;

        if(current == q->tail)
            q->tail = prev;
        //mutex_unlock(&m);
        free(current);
        return fd;
    }

    return -1;
}


void freeQueue(struct Queue* q)
{
    if (q == NULL) {
        return;
    }

    while (q->head != NULL) {
        struct Node* tmp = q->head;
        q->head = q->head->next;

        free(tmp);
    }

    if (q->tail != NULL) {
        free(q->tail);
    }

    free(q);
}
int queue_empty(struct Queue* q) {
    return q->size == 0;
}

int get_queue_size(struct Queue* q){
    return q->size;
}

struct timeval get_head_arrival(struct Queue* q){
    if(!queue_empty(q))
        return (q->head)->arrival_time;
}