#ifndef CLIENT_THREADPOOL_H
#define CLIENT_THREADPOOL_H

#define THREAD_POOL_SIZE 5

extern pthread_t threads[THREAD_POOL_SIZE];
extern int thread_active[THREAD_POOL_SIZE];
extern int socket_queue[THREAD_POOL_SIZE];
extern int queue_size;
extern pthread_mutex_t queue_mutex;
extern pthread_cond_t queue_cond;

void init_client_threadpool();
void* client_thread(void* arg);
void cleanup_client_threadpool();
int dequeue_socket();
void enqueue_socket(int sockfd);

#endif