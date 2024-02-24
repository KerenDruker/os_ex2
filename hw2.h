#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#define MAX_THREADS 4096
#define MAX_COMMAND_LENGTH 1024
#define MAX_COUNTERS 100
#define MAX_COMMANDS 1000

typedef struct job {
    char command[MAX_COMMAND_LENGTH];
    struct job *next;
} job_t;

// Global variables
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t work_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t task_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_tasks_completed_cond = PTHREAD_COND_INITIALIZER;
job_t *work_queue_head = NULL;
job_t *work_queue_tail = NULL;

int shutdown_requested = 0; // Global flag to control thread lifecycle
pthread_mutex_t shutdown_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to protect access to the flag

int active_tasks = 0;
int log_enabled = 0;
long long hw2_start;

// functions declarations
void execute_worker_cmd(char *cmd);
void shutdown_work_threads(pthread_t* threads, int num_threads);
void* worker(void* arg);
void enqueue_job(const char* command);
job_t* dequeue_job();
void complete_job();
void waitForAllJobsToComplete();