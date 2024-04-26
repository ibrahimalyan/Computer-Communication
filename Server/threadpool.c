//
// Created by student on 3/12/24.
//
#include <stdlib.h>
#include <stdio.h>
#include "threadpool.h"
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

// Global variable to track the number of completed tasks
size_t completed_tasks = 0;
bool terminate_program = false; // Indicates whether the program should terminate
size_t max_num_of_tasks; // Global variable to store the maximum number of tasks

void* do_work(void* p);

threadpool* create_threadpool(int num_threads_in_pool) {
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL) {
        fprintf(stderr, "Usage: pool <pool-size> <number-of-tasks> <max-number-of-request>\n");
        return NULL;
    }

    threadpool* t_pool = (threadpool*)malloc(sizeof(threadpool));
    if (t_pool == NULL) {
        perror("error: <sys_call>\n");
        return NULL;
    }

    t_pool->num_threads = num_threads_in_pool;
    t_pool->qsize = 0;
    t_pool->qhead = NULL;
    t_pool->qtail = NULL;
    t_pool->shutdown = 0;
    t_pool->dont_accept = 0;

    if (pthread_mutex_init(&(t_pool->qlock), NULL) != 0) {
        perror("error: <sys_call>\n");
        free(t_pool);
        return NULL;
    }

    if (pthread_cond_init(&(t_pool->q_empty), NULL) != 0 ||
        pthread_cond_init(&(t_pool->q_not_empty), NULL) != 0) {
        perror("error: <sys_call>\n");
        pthread_mutex_destroy(&(t_pool->qlock));
        free(t_pool);
        return NULL;
    }

    t_pool->threads = (pthread_t*)malloc(num_threads_in_pool * sizeof(pthread_t));
    if (t_pool->threads == NULL) {
        perror("error: <sys_call>\n");
        pthread_mutex_destroy(&(t_pool->qlock));
        pthread_cond_destroy(&(t_pool->q_empty));
        pthread_cond_destroy(&(t_pool->q_not_empty));

        free(t_pool);
        return NULL;
    }

    for (int i = 0; i < num_threads_in_pool; ++i) {
        if (pthread_create(&(t_pool->threads[i]), NULL, do_work, (void*)t_pool) != 0) {
            perror("error: <sys_call>\n");
            // Clean  any threads
            for (int j = 0; j < i; ++j) {
                pthread_cancel(t_pool->threads[j]);
                pthread_join(t_pool->threads[j], NULL);
            }
            pthread_mutex_destroy(&(t_pool->qlock));
            pthread_cond_destroy(&(t_pool->q_empty));
            pthread_cond_destroy(&(t_pool->q_not_empty));
            free(t_pool->threads);
            free(t_pool);
            return NULL;
        }
    }

    return t_pool;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg) {
    if (from_me == NULL || dispatch_to_here == NULL) {
        fprintf(stderr, "Usage: pool <pool-size> <number-of-tasks> <max-number-of-request>\n");
        return;
    }

    work_t* work = (work_t*)malloc(sizeof(work_t));
    if (work == NULL) {
        perror("error: <sys_call>\n");
        return;
    }
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;

    pthread_mutex_lock(&(from_me->qlock));

    if (from_me->qsize == 0) {
        from_me->qhead = work;
        from_me->qtail = work;
        pthread_cond_signal(&(from_me->q_not_empty));
    } else {
        from_me->qtail->next = work;
        from_me->qtail = work;
    }
    from_me->qsize++;

    pthread_mutex_unlock(&(from_me->qlock));
}


void* do_work(void* p) {
    threadpool* t_pool = (threadpool*)p;

    while (1) {
        pthread_mutex_lock(&(t_pool->qlock));

        while (t_pool->qsize == 0 && !t_pool->shutdown) {
            pthread_cond_wait(&(t_pool->q_not_empty), &(t_pool->qlock));
        }

        if (t_pool->shutdown && t_pool->qsize == 0) {
            pthread_mutex_unlock(&(t_pool->qlock));
            pthread_exit(NULL);
        }

        work_t* work = t_pool->qhead;
        if (work == NULL) {
            pthread_mutex_unlock(&(t_pool->qlock));
            continue;
        }

        t_pool->qhead = work->next;
        if (t_pool->qhead == NULL) {
            t_pool->qtail = NULL;
        }
        t_pool->qsize--;

        pthread_mutex_unlock(&(t_pool->qlock));

        (*(work->routine))(work->arg);
        free(work);

        // Increment completed tasks
        pthread_mutex_lock(&t_pool->qlock);
        completed_tasks++;
        pthread_mutex_unlock(&t_pool->qlock);

        // Check if all tasks are completed
        if (completed_tasks >= max_num_of_tasks) {
            // Set the flag to terminate the program
            terminate_program = true;
            // Signal the main thread to terminate
            pthread_cond_signal(&(t_pool->q_empty));
            break;
        }
    }

    return NULL;
}

void destroy_threadpool(threadpool* destroyme) {
    if (destroyme == NULL) {
        fprintf(stderr, "Usage: pool <pool-size> <number-of-tasks> <max-number-of-request>\n");
        return;
    }

    pthread_mutex_lock(&(destroyme->qlock));
    destroyme->dont_accept = 1;
    while (destroyme->qsize > 0) {
        pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock));
    }
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(&(destroyme->qlock));

    for (int i = 0; i < destroyme->num_threads; ++i) {
        pthread_cancel(destroyme->threads[i]);
        pthread_join(destroyme->threads[i], NULL);
    }

    free(destroyme->threads);
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    pthread_cond_destroy(&(destroyme->q_empty));
    free(destroyme);
}

void* Task(void* arg) {
    printf("THREAD_ID: %lu\n", (unsigned long)pthread_self());
    usleep(100000); // Sleep for 100 milliseconds (100000 microseconds)
    return NULL; // Return NULL as the function signature is void* and this function does not return a value
}

//int main(int argc, char * argv[]){
//    size_t pool_size = atoi(argv[1]);
//    size_t num_of_tasks = atoi(argv[2]);
//    max_num_of_tasks = atoi(argv[3]);
//
//
//    threadpool* t_pool = create_threadpool(pool_size);
//
//    if (t_pool == NULL) {
//        fprintf(stderr, "Usage: pool <pool-size> <number-of-tasks> <max-number-of-request>\n");
//        return 1;
//    }
//
//    for (size_t i = 0; i < max_num_of_tasks; i++) {
//        if (i >= num_of_tasks) break; // Exit loop if reached the specified number of tasks
//
//        dispatch(t_pool, (dispatch_fn)Task, NULL);
//    }
//
//    // Wait until all tasks are completed
//    pthread_mutex_lock(&t_pool->qlock);
//    while (!terminate_program) {
//        pthread_cond_wait(&t_pool->q_empty, &t_pool->qlock);
//    }
//    pthread_mutex_unlock(&t_pool->qlock);
//
//    destroy_threadpool(t_pool);
//
//    // Terminate the program
//    printf("All threads have terminated. Exiting...\n");
//    return 0;
//}