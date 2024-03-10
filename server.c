#include "segel.h"
#include "request.h"

#include "Queue.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

/*-------------------------------------STRUCTS&ENUMS-------------------------------------*/
typedef enum OverloadPolicy {
    BLOCK, BLOCK_FLUSH, DROP_TAIL, DROP_HEAD, DROP_RANDOM
} OverloadPolicy;

/*-------------------------------------GLOBAL VARIABLES-------------------------------------*/
int max_total_request, total_workers;
int* dynamic_stats;
int* static_stats;
int* total_stats;

Queue working_queue = NULL;
Queue wait_queue = NULL;

pthread_cond_t main_block_cond;
pthread_cond_t queue_cond;
pthread_mutex_t requests_lock;

OverloadPolicy schedalg;

/*-------------------------------------WORKERS-------------------------------------*/
void* thread_main(void* thread_args){
    int thread_id = ((int *)thread_args)[0];
    struct timeval arrival_time;

    while(1){
        pthread_mutex_lock(&requests_lock);

        while(queue_empty(wait_queue)){
            pthread_cond_wait(&queue_cond, &requests_lock);
        }

        arrival_time = get_head_arrival(wait_queue);
        int current_request = dequeue(wait_queue);

        //printf("WORKING THREAD TOOK %d FROM WAITING QUEUE\n", current_request);

        enqueue(working_queue, current_request, arrival_time);
        pthread_mutex_unlock(&requests_lock);

        //printf("WORKING THREAD INSERTED %d INTO WORKING QUEUE\n", current_request);

        struct timeval dispatch_time;
        gettimeofday(&dispatch_time, NULL);

        requestHandle(current_request, thread_id, static_stats, dynamic_stats, total_stats, arrival_time, dispatch_time);
        Close(current_request);

        pthread_mutex_lock(&requests_lock);
        dequeue_by_fd(working_queue, current_request);
        pthread_cond_signal(&main_block_cond);

        pthread_mutex_unlock(&requests_lock);

        //printf("WORKING THREAD FINISHED HANDLING %d AND TOOK IT OUT OF THE WORKING QUEUE\n", current_request);

    }
    return NULL;
}

void init_workers(int workers_num, int queue_size){
    pthread_t* workers = malloc(sizeof(*workers)*workers_num);

    for(int i = 0; i < workers_num; i++){
        int thread_args[] = {i, };
        pthread_create(&workers[i], NULL, thread_main, (void *)thread_args);
    }

}

/*-------------------------------------MAIN-------------------------------------*/
void getargs(int *port, int *thread, int *queue_size, OverloadPolicy *schedalg, int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <portnum> <threads> <queue_size> <schedalg>\n", argv[0]);
        exit(1);
    }

    *port = atoi(argv[1]);
    *thread = atoi(argv[2]);
    *queue_size = atoi(argv[3]);

    if (strcmp(argv[4], "block") == 0)
        *schedalg = BLOCK;
    else if (strcmp(argv[4], "dt") == 0)
        *schedalg = DROP_TAIL;
    else if (strcmp(argv[4], "dh") == 0)
        *schedalg = DROP_HEAD;
    else if (strcmp(argv[4], "random") == 0)
        *schedalg = DROP_RANDOM;
    else if (strcmp(argv[4], "bf") == 0)
        *schedalg = BLOCK_FLUSH;
    else
        app_error("Scheduling algorithm error");

    if (*port <= 0)
        app_error("Port number error");
    if (*thread <= 0)
        app_error("Thread number error");
    if (*queue_size <= 0)
        app_error("Queue size number error");
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, thread, queue_size, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, &thread, &queue_size, &schedalg, argc, argv);

    /*Updating global variables*/
    total_workers = thread;
    max_total_request = queue_size;

    /*Setting locks*/
    pthread_mutex_init(&requests_lock, NULL);
    pthread_cond_init(&queue_cond, NULL);
    pthread_cond_init(&main_block_cond, NULL);

    /*Initializing working threads&request queues*/
    working_queue = newQueue(total_workers);
    wait_queue = newQueue(max_total_request);
    init_workers(total_workers, queue_size);

    /*Initializing statistics arrays*/
    dynamic_stats = malloc(sizeof(int) * total_workers);
    static_stats = malloc(sizeof(int) * total_workers);
    total_stats = malloc(sizeof(int) * total_workers);

    for(int i = 0; i<total_workers; i++){
        dynamic_stats[i] = 0;
        static_stats[i] = 0;
        total_stats[i] = 0;
    }
    /*Request handling*/
    listenfd = Open_listenfd(port);
    struct timeval arrival_time;

    //printf("STARTED LISTENING FOR REQUESTS!\n");

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        gettimeofday(&arrival_time, NULL);

        //printf("GOT A REQUEST: %d!\n", connfd);
        int toDrop;
        int index;
        pthread_mutex_lock(&requests_lock);

        /*Dispatch request by policy*/
        if(get_queue_size(working_queue)+ get_queue_size(wait_queue) >= max_total_request){
            switch(schedalg){
                case BLOCK:
                    while(get_queue_size(working_queue) + get_queue_size(wait_queue) == queue_size){
                        pthread_cond_wait(&main_block_cond, &requests_lock);
                    }
                    break;
                case DROP_TAIL:
                    Close(connfd);
                    pthread_mutex_unlock(&requests_lock);
                    continue;                           //TODO: check if skips the iteration of outer while loop
                    //break;
                case DROP_HEAD:
                    if(queue_empty(wait_queue)){
                        Close(connfd);
                        pthread_mutex_unlock(&requests_lock);
                        continue;                       //TODO: check if skips the iteration of outer while loop
                    } else {
                        int fd = dequeue(wait_queue);
                        Close(fd);
                    }
                    break;
                case BLOCK_FLUSH:
                    while(get_queue_size(working_queue) + get_queue_size(wait_queue)!= 0){
                        pthread_cond_wait(&main_block_cond, &requests_lock);
                    }
                    continue;                           //TODO: check if skips the iteration of outer while loop
                    //break;
                case DROP_RANDOM:
                    toDrop = ceil(get_queue_size(wait_queue) / 2);
                    for(int i = 0; i < toDrop; i++){
                       if(queue_empty(wait_queue))
                           break;

                       index = rand() % get_queue_size(wait_queue);
                       int fd = dequeue_by_index(wait_queue, index);
                       Close(fd);
                    }
                    break;

                default:
                    app_error("Wrong scheduling algorithm");
            }
        }

        //printf("INSERTING %d INTO WAITING QUEUE\n", connfd);
        enqueue(wait_queue, connfd, arrival_time);

        pthread_cond_signal(&queue_cond);
        pthread_mutex_unlock(&requests_lock);
    }
}