#include "UMSLibrary.h"

//If this is not added, doxygen parse the next two lines in a "strange" way
#ifndef DOXYGEN_SHOULD_SKIP_THIS
__attribute__((section(".init_array"))) typeof(UMS_init) *__init = UMS_init;
__attribute__((section(".fini_array"))) typeof(UMS_exit) *__fini = UMS_exit;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


//descriptor of the device file
int fd;

/*variables and semaphores used to start the schedulers;
the schedulers wait for all the threads to be initialized before starting
this is done to avoid problems like a scheduler starting with 0 thread,
that will lead to an early finish of the scheduler
*/
int worker_num, loaded_num;
sem_t num_sem;


/**
 * @fn UMS_init()
 * Initialize the connection with the kernel module; the kernel module need
 * to be already loaded when starting your application. This function is inserted in the section
 * init_array, thus it will be called before the main function; be carefull while modifying that section.
 * 
 */

void UMS_init(){
    int ret;

    if( access( DEVICE_PATH, F_OK ) != 0 ) {
        printf("Device file not found! Is the kernel module loaded?\n");
        exit(UMS_ERROR_INIT);
    }

    fd = open("/dev/ums-dev", 0);
    if(fd == -1){
        printf("Could not open the device file! Aborting.\n");
        exit(UMS_ERROR_FD);
    }

    DO_IOCTL(fd, INIT_UMS_PROCESS, 0);

    ret = sem_init(&num_sem, 0, 1);
    if(ret == -1){
        printf("Could not initialize the semaphore Aborting");
        exit(UMS_ERROR_SEM);
    }

}

/**
 * @fn UMS_exit()
 * Remove the connection with the kernel module.
 * Tells the kernel module that the process is exiting, and closes the file descriptor of the device file.
 * This function is inserted in the section fini_array so that it will be called after the main function ended.
 */
void UMS_exit(){
    int ret;

    ret = sem_destroy(&num_sem);
    if(ret == -1){
        printf("Could not remove the semaphore, Aborting");
        exit(UMS_ERROR_SEM);
    }
    
    DO_IOCTL(fd, EXIT_UMS_PROCESS, 0);

    ret = close(fd);
    if(ret == -1){
        printf("Could not close the device file properly.\n");
        exit(UMS_ERROR_FD);
    }

}


/**
 *
 * @p list the completion list of the scheduler \n 
 * @p start_routine the function that will execute the scheduler \n 
 * @p arg the argument of the scheduler's function \n 
 * 
 * Create a scheduler thread that will execute the given function, wit the given arguments.
 * The return value is the ID of the scheduler.
 * 
 */
ums_t EnterUmsSchedulingMode(void* list, void *(*start_routine) (completion_list *, void *), void* arg){
    ums_t id;

    //printf("Creating scheduler thread.\n");

    shceduling_wrapper_routine_arg* wrapper_arg = (shceduling_wrapper_routine_arg*) malloc(sizeof(shceduling_wrapper_routine_arg));
    wrapper_arg->start_routine = start_routine;
    wrapper_arg->arg = arg;
    wrapper_arg->list=list;
    
    pthread_create(&id, NULL, SchedulerThreadWrapper, wrapper_arg);

    return id;
}



/**
 * @p arg the argument passed from the user, for the scheduler function
 * 
 * Contacts the kernel module to communicate that a new scheduler has spawned (also communicating the completion list)
 * then wait for all the workers to start and then it calls the user-defined scheduler's function.
 */

void* SchedulerThreadWrapper(void* arg){
    int ret;

    shceduling_wrapper_routine_arg* wrapper_arg = (shceduling_wrapper_routine_arg*) arg;

    while (worker_num != loaded_num){}

    completion_list *cs = wrapper_arg->list;
    unsigned long memory[cs->len + 1];
    memory[0] = cs->len;
    int i;

    completion_list_item* item = cs->head;
    completion_list_item* aux;

    sem_wait(&cs->sem);

    for(i = 1; i<=cs->len; i++){

        memory[i] = item->ums_id;

        item = item->next;
    }
    sem_post(&cs->sem);

    DO_IOCTL(fd, INTRODUCE_UMS_SCHEDULER, &memory);

    wrapper_arg->start_routine(wrapper_arg->list, wrapper_arg->arg);


    free(arg);
    pthread_exit(0);

}

/**
 *
 * @p start_routine the function that will execute the worker\n
 * @p arg the argument of the worker's function\n
 * 
 *  Create a worker thread that will execute the given function, wit the given arguments.
 * The return value is the ID of the worker, this value is used to point to a worker.
 * 
 */
ums_t EnterUmsWorkingMode(void *(*start_routine) (void *), void* arg){
    ums_t id;

    //printf("Creating working thread.\n");

    //update the counter, no need for locks here, it is done sequentially
    worker_num += 1;
    //printf("Current number of workers:%d\n", worker_num);

    working_wrapper_routine_arg* wrapper_arg = (working_wrapper_routine_arg*) malloc(sizeof(working_wrapper_routine_arg));
    wrapper_arg->start_routine = start_routine;
    wrapper_arg->arg = arg;

    pthread_create(&id, NULL, WorkingThreadWrapper, (void*) wrapper_arg);

    return id;
}

/**
 * @p arg the argument passed from the user, for the worker function
 * 
 * Contacts the kernel module to communicate that a new worker has spawned
 * then it executes the worker function. When the worker function ends, the kernel module is notified.
 */

void* WorkingThreadWrapper(void* arg){
    int ret;

    working_wrapper_routine_arg* wrapper_arg = (working_wrapper_routine_arg*) arg;

    ums_t ums_id = ums_get_id();

    //update counter
    sem_wait(&num_sem);
    loaded_num += 1;
    sem_post(&num_sem);

    DO_IOCTL(fd, INTRODUCE_UMS_TASK, &ums_id);

    wrapper_arg->start_routine(wrapper_arg->arg);

    DO_IOCTL(fd, UMS_WORKER_DONE, 0);

    free(arg);
    pthread_exit(0);

}

/**
 * @p id the id of the thread that needs to be executed
 * 
 * Called from a scheduler thread, this function will execute a worker thread. The scheduler thread will remain blocked untill
 * the worker thread will yield, or end.
 */

void ExecuteUmsThread(ums_t id){

    DO_IOCTL(fd, EXECUTE_UMS_THREAD, (unsigned long*) &id);
}

/**
 * @fn UmsThreadYield
 * 
 * Called from a worker thread, pauses the execution and gives back the control to its (last) scheduler.
 */
void UmsThreadYield(){

    DO_IOCTL(fd, UMS_THREAD_YIELD, 0);

}

/**
 * @p cs the complition list of the scheduler
 * 
 * This function returns a completion list of all ready thread to be executed among those which are present in the completion list
 * given in input. The returned list must be deleted by the user using the function completion_list_delete(), otherwise leaks
 * will occur.
 */
completion_list* DequeueUmsCompletionListItems(completion_list* cs){

    unsigned long memory[cs->len + 1];
    memory[0] = cs->len;
    int i;

    completion_list_item* item = cs->head;
    completion_list_item* aux;

    sem_wait(&cs->sem);

    for(i = 1; i<=cs->len; i++){

        memory[i] = item->ums_id;

        item = item->next;
    }
    sem_post(&cs->sem);

    DO_IOCTL(fd, UMS_DEQUEUE, &memory);

    completion_list* ready = completion_list_create();
    
    item = cs->head;

    for(i = 1; i<=cs->len; i++){    
        //printf("Mem[%d]=%lu", i, memory[i]);
        if(memory[i]){
            completion_list_add(ready, memory[i], item->prio);
        }
        item = item->next;
    }
    //printf("\n");

    return ready;
}

/**
 * @p thread the thread ID of the thread
 * @p retval a pointer in which the return value will be saved. If null (i.e. 0) will be passed, the value will not be saved
 * 
 * Waits for the completion of the execution of the given thread.
 */
int ums_thread_join(ums_t thread, void **retval){
    return pthread_join(thread, retval);
}
/**
 * @fn ums_get_id
 * 
 * Returns the id of the caller thread.
 */
ums_t ums_get_id(){
    return pthread_self();
}