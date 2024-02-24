
#include "hw2.h"
// help functions: 

long long getCurrentTimeMilliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

void removeExtraSpaces(const char *src, char *dest) {
    const char *current = src;
    int inWord = 0; // Flag to track whether we're inside a word

    while (*current != '\0') {
        if (*current == '\t') { // Check for tabs and treat them as a single space
            // Convert tab to space and ensure only one space is added between words
            if (inWord) {
                *dest++ = ' ';
                inWord = 0;
            }
        } else if (isspace((unsigned char)*current)) {
            // Handle other whitespace (spaces, newlines) similarly
            if (inWord) {
                *dest++ = ' ';
                inWord = 0;
            }
        } else {
            *dest++ = *current;
            inWord = 1;
        }
        current++;
    }

    // Remove trailing space
    if (dest > src && isspace((unsigned char)*(dest - 1))) {
        dest--;
    }

    *dest = '\0'; // Null-terminate the result string
}

void separateCommands(const char* command, char commands[MAX_COMMANDS][MAX_COMMAND_LENGTH]) {
    int i = 0;
    char commandCopy[MAX_COMMAND_LENGTH];
    strncpy(commandCopy, command, MAX_COMMAND_LENGTH);
    commandCopy[MAX_COMMAND_LENGTH - 1] = '\0';

    char* token = strtok(commandCopy, ";");

    while (token != NULL && i < MAX_COMMANDS) {
        // Trim leading and trailing spaces if necessary adds '\0' at the end.
        snprintf(commands[i], MAX_COMMAND_LENGTH, "%s", token);
        i++;
        token = strtok(NULL, ";");
    }
}

void modifyCounter(int counterNumber, int delta) {
    char filename[50];
    snprintf(filename, sizeof(filename), "count%02d.txt", counterNumber);

    FILE* file = fopen(filename, "r+");
    if (!file) {
        printf("Failed to open counter file");
        exit(1);
    }

    long long currentValue = 0;
    fscanf(file, "%lld", &currentValue); // Read current value
    currentValue += delta; // Increment or decrement

    // Move to the beginning of the file for writing
    fseek(file, 0, SEEK_SET);
    fprintf(file, "%lld", currentValue); // Write updated value
    fclose(file);
}

void increment(int x) {
    modifyCounter(x, 1); // Increment the counter by 1
}

void decrement(int x) {
    modifyCounter(x, -1); // Decrement the counter by 1
}

void execute_worker_cmd(char *cmd) {
    if (strncmp(cmd, "msleep", 6) == 0) {
        int milliseconds = atoi(cmd + 7);
        usleep(milliseconds * 1000);  // sleep for x miliseconds
    } else if (strncmp(cmd, "increment", 9) == 0) {
        int counterNumber = atoi(cmd + 10);
        increment(counterNumber);
    } else if (strncmp(cmd, "decrement", 9) == 0) {
        int counterNumber = atoi(cmd + 10);
        decrement(counterNumber);
    }
    printf("debug: Command %s: \n", cmd);
}

void shutdown_work_threads(pthread_t* threads, int num_threads) {
    // Signal shutdown
    pthread_mutex_lock(&shutdown_mutex);
    shutdown_requested = 1;
    pthread_mutex_unlock(&shutdown_mutex);

    // Wake up all waiting threads
    pthread_cond_broadcast(&work_cond);

    // Wait for threads to finish
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}


void* worker(void* arg) {
    int thread_id = *(int*)arg;
    FILE* log_file = NULL;
    char log_filename[20];

    if (log_enabled) {
        snprintf(log_filename, sizeof(log_filename), "thread%02d.txt", thread_id);
        log_file = fopen(log_filename, "w");
        if (!log_file) {
            perror("Failed to open log file");
            exit(1);
        }
    }

    while (1) { // TODO: Use some condition to break loop when needed
        char commands_seq_arr[MAX_COMMANDS][MAX_COMMAND_LENGTH];
        pthread_mutex_lock(&shutdown_mutex);
        if (shutdown_requested) {
            pthread_mutex_unlock(&shutdown_mutex);
            break; // Exit the loop (and the thread) if shutdown is requested
        }
        pthread_mutex_unlock(&shutdown_mutex);

        pthread_mutex_lock(&queue_mutex);
        while (work_queue_head == NULL) {
            /* Wait for condition variable COND to be signaled or broadcast.
            MUTEX is assumed to be locked before.
            This function is a cancellation point and therefore not marked with __THROW.  */
            pthread_cond_wait(&work_cond, &queue_mutex);
            
            // Re-check the shutdown flag after waking up
            pthread_mutex_lock(&shutdown_mutex);
            if (shutdown_requested) {
                pthread_mutex_unlock(&shutdown_mutex);
                pthread_mutex_unlock(&queue_mutex);
                return NULL; // Exit the thread
            }
            pthread_mutex_unlock(&shutdown_mutex);
        }
        
        job_t* job = dequeue_job();
        pthread_mutex_unlock(&queue_mutex);
        
        if (job == NULL) continue;
        
        long long job_start_time = getCurrentTimeMilliseconds() - hw2_start;
        if (log_enabled && log_file) {
            fprintf(log_file, "TIME %lld: START job %s\n", job_start_time, job->command);
            fflush(log_file); // Ensure it's written immediately
        }

        separateCommands(job->command, commands_seq_arr);

        for (int i = 0; commands_seq_arr[i][0] != '\0' && i < MAX_COMMANDS; i++) {
            
            // if the command is repeat needs to repeat the rest of the command x times.
            if (strncmp(commands_seq_arr[i], "repeat", 6) == 0) {

                int times = atoi(commands_seq_arr[i] + 7);
                for (int r = 0; r < times; r++) {
                    // execute all the commands after the repeat command
                    for (int j = i + 1; commands_seq_arr[j][0] != '\0' && j < MAX_COMMANDS; j++) {
                        // TODO: Execute command at commands_seq_arr[j] (not repeat)
                        printf("repet %d", times);
                        execute_worker_cmd(commands_seq_arr[j]);
                    }
                }
                break;
            } 
            // not a reppeat command need to execeute once
            else {
                // TODO: Execute command at commands_seq_arr[i]
                execute_worker_cmd(commands_seq_arr[i]);
            }
            long long job_end_time = getCurrentTimeMilliseconds() - hw2_start;
            if (log_enabled && log_file) {
                fprintf(log_file, "TIME %lld: END job %s\n", job_end_time, commands_seq_arr[i]);
                fflush(log_file); // Ensure it's written immediately
            }
        }
        
        free(job);
        complete_job();
    }

    if (log_file) {
        fclose(log_file);
    }
    return NULL;
}

int readLineFromCmdFile(FILE *cmdfile, char *command){
    char tempBuffer[MAX_COMMAND_LENGTH];
    
    if (fgets(tempBuffer, MAX_COMMAND_LENGTH, cmdfile) != NULL) {
        // Handle dispatcher-specific commands...
        size_t len = strlen(tempBuffer);
        if (len > 0 && tempBuffer[len - 1] == '\n') {
            tempBuffer[len - 1] = '\0';
        }
        removeExtraSpaces(tempBuffer, command);
        return 1;
    }
    return 0;
}

void enqueue_job(const char* command) {
    job_t* job = malloc(sizeof(job_t));
    strcpy(job->command, command);
    job->next = NULL;
    
    // adding to a queue is a critical section beacuse it involses 
    // saving the last elemt and and adding acoodringly. 
    /* Lock a mutex.  */
    pthread_mutex_lock(&queue_mutex);
    if (work_queue_head == NULL) {
        work_queue_head = job;
        work_queue_tail = job;
    } else {
        work_queue_tail->next = job;
        work_queue_tail = job;
    }
    /* Wake up one thread waiting for condition variable COND.  */
    pthread_cond_signal(&work_cond);
    /* Unlock a mutex.  */
    pthread_mutex_unlock(&queue_mutex);
} 


// deueue the first element of the queue. 
job_t* dequeue_job() {
    // adding to a queue is a critical section beacuse it involses 
    // changing the head elemt
    /* Lock a mutex.  */
    pthread_mutex_lock(&queue_mutex);
    pthread_mutex_lock(&task_count_mutex);

    if (work_queue_head == NULL) {
        /* Unlock a mutex.  */
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }    
    
    job_t* job = work_queue_head;
    work_queue_head = work_queue_head->next;
    // if the queue is empty
    if (work_queue_head == NULL) {
        work_queue_tail = NULL;
    }
    active_tasks++; // Increment active task count
    pthread_mutex_unlock(&queue_mutex);
    pthread_mutex_unlock(&task_count_mutex);
    return job;
}

void complete_job() {
    pthread_mutex_lock(&task_count_mutex);
    active_tasks--; // Decrement active task count

    if (active_tasks == 0) {
        // Signal dispatcher if all tasks are completed
        pthread_cond_signal(&all_tasks_completed_cond);
    }

    pthread_mutex_unlock(&task_count_mutex);
}



void waitForAllJobsToComplete() {
    // Wait for all pending background commands to complete before continuing to
    // process the next line in the input file.
    // Wait for threads to finish
    pthread_mutex_lock(&task_count_mutex);

    while (active_tasks > 0) {
        // Wait for signal that tasks are completed
        pthread_cond_wait(&all_tasks_completed_cond, &task_count_mutex);
    }

    pthread_mutex_unlock(&task_count_mutex);
}

int main(int argc, char *argv[]) {
    char counter_xx[20];
    FILE *fp_counter;
    long long counter = 0;
    FILE* dispatcher_log_file = NULL;

    hw2_start = getCurrentTimeMilliseconds();
    //  Analyze the command line arguments 
    if (argc != 5) {
        printf("please enter the correct number of argumnets for hw2");
        exit(1);
    }

    // implement the arguments and validate them
    
    int num_threads = atoi(argv[2]);
    if (num_threads > MAX_THREADS) {
        printf("Too many threads");
        exit(1);
    }

    int num_counters = atoi(argv[3]);
    if (num_counters > MAX_COUNTERS) {
        printf("Too many counters");
        exit(1);
    }

    log_enabled = atoi(argv[4]);
    if (log_enabled != 0 && log_enabled != 1) {
        printf("Invalid log_enabled flag should be boolean");
        exit(1);
    }

    if (log_enabled) {
    dispatcher_log_file = fopen("dispatcher.txt", "w");
    if (!dispatcher_log_file) {
        printf("Failed to open dispatcher log file");
        exit(1);
    }
}
    // Create num counters “counter files”
    for (int i = 0; i < num_counters; i++) {
        sprintf(counter_xx, "count%02d.txt", i);
        fp_counter = fopen(counter_xx,"w");
        if (fp_counter == NULL) {
            printf("Failed to create file %s\n", counter_xx);
            exit(1);
        }
        fprintf(fp_counter, "%lld", counter); 
        fclose(fp_counter);        
    }
    
    //  Create num threads new worker threads
    pthread_t threads[num_threads];
    int thread_ids[num_threads];
    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, worker, (void*)&thread_ids[i]) != 0) {
            printf("Failed to create worker thread");
            exit(1);
        }
    }
    
    // Dispatcher reads command file and queues jobs
    char command[MAX_COMMAND_LENGTH];
    FILE *cmdfile = fopen(argv[1], "r");
    
    if (!cmdfile) {
        printf("Failed to open command file");
        exit(1);
    }

    while(readLineFromCmdFile(cmdfile, command)) {

        if (log_enabled) {
            long long time_now = getCurrentTimeMilliseconds();
            fprintf(dispatcher_log_file, "TIME %lld: read cmd line: %s\n", time_now-hw2_start, command);
        }

        // sort the commands
        if (strncmp(command, "dispatcher_msleep", 17) == 0) {
            // Extract x from the command
            long x = atol(command + 18);
            usleep(x * 1000); // usleep - sleep for # microseconds * 1000 convert to miliseconds
        } else if (strncmp(command, "dispatcher_wait", 15) == 0) {
            waitForAllJobsToComplete();
        } else if (strncmp(command, "worker", 6) == 0) {
            enqueue_job(command);
        }
    }
    fclose(cmdfile);
    fclose(dispatcher_log_file);

    
    // Wait for worker threads to complete...
    
    // Cleanup and exit
  
    // TODO: close all threads 
    // jion all therad and make sure they are done before shutting dowm.
    shutdown_work_threads(threads, num_threads);
    return 0;
}
