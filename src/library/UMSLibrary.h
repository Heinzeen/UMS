/**
 * @file UMSLibrary.h
 * @brief Header for UMSLibrary.h
 * 
 * Main definitions and functionalities of the user-side library for UMS scheduling
 */
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <semaphore.h>

#include "UMSList.h"



#define INIT_UMS_PROCESS            0
#define EXIT_UMS_PROCESS            1
#define INTRODUCE_UMS_TASK          3
#define INTRODUCE_UMS_SCHEDULER     4
#define EXECUTE_UMS_THREAD          5
#define UMS_THREAD_YIELD            6
#define UMS_WORKER_DONE             7
#define UMS_DEQUEUE                 8

#define UMS_ERROR_INIT              -1
#define UMS_ERROR_IOCTL             -2
#define UMS_ERROR_SEM               -3
#define UMS_ERROR_FD                -4

#define DEVICE_NAME "ums-dev"
#define DEVICE_FOLDER "/dev/"
#define DEVICE_PATH DEVICE_FOLDER DEVICE_NAME

/**
 * This macro is used to speed up the call to IOCTL. The communication between the user application
 * and the kernel module are done through the use of this system call.
 */
#define DO_IOCTL(fd, type, arg)\
do{\
    if(arg != 0){\
        if(ioctl(fd, type, arg) == -1){\
            printf("Could not perform ioctl! Aborting\n");\
            exit(UMS_ERROR_IOCTL);\
        }\
    }\
    else{\
        if(ioctl(fd, type) == -1){\
            printf("Could not perform ioctl! Aborting\n");\
            exit(UMS_ERROR_IOCTL);\
        }\
    }\
}while(0)

typedef pthread_t ums_t;

/**
 * for internal use only
 */
typedef struct working_wrapper_routine_arg{
    void *(*start_routine) (void *);
    void* arg;
    int fd;
}working_wrapper_routine_arg;

/**
 * for internal use only
 */
typedef struct shceduling_wrapper_routine_arg{
    void *(*start_routine) (completion_list *, void*);
    void* arg;
    completion_list* list;
    int fd;
}shceduling_wrapper_routine_arg;


void UMS_init(void);
void UMS_exit(void);

//user interface
ums_t EnterUmsSchedulingMode(void*, void *(*start_routine) (completion_list *, void *), void* );
ums_t EnterUmsWorkingMode(void *(*start_routine) (void *), void* );
void ExecuteUmsThread(ums_t);
void UmsThreadYield(void);
completion_list* DequeueUmsCompletionListItems(completion_list*);
int ums_thread_join(ums_t thread, void **retval);
ums_t ums_get_id(void);


//wrappers
void* WorkingThreadWrapper(void*);
void* SchedulerThreadWrapper(void*);

