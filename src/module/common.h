/**
 * @file common.h
 * @brief Header that defines some struct shared by all files
 * 
 * This header defines the core definitions used by all the files of the project.
 */
#include <linux/init.h>


/**
 * @p id the id of the thread \n 
 * @p task_struct pointer to the thread's task struct \n 
 * @p scheduler pointer to the (last) scheduler of the thread \n 
 */
typedef struct thread_item
{
        unsigned long id;
        struct task_struct* task_struct;
        struct task_struct* scheduler;
        struct list_head list;
} thread_item;

/**
 * @p id the id of the thread (from 0 to n) \n 
 * @p ums_id the id of the thread (as given by the threads' implementation) \n 
 * @p state the state of the thread, 1 is running and 0 is idle \n 
 * @p counter the counter of the times this thread had been switched in \n 
 */
typedef struct worker_info
{
        int id;
        unsigned long ums_id;
        int state;
        int counter;
        struct list_head list;
}worker_info;

/**
 * @p ums_id the id of the scheduler (as given by the threads' implementation) \n 
 * @p worker_num the number of the workers in the completion list \n 
 * @p task_struct pointer to the thread's task struct \n 
 * @p dir pointer to the scheduler/id directory \n 
 * @p workers pointer to the scheduler/id/workers/ directory \n 
 * @p info pointer to the scheduler/id/info file \n 
 * @p counter total number of switches \n 
 * @p total_time the sum of the time needed to do the switches (used to compute the avg) \n 
 * @p time the time needed for the last switch \n 
 * @p last_time auxiliary field used to compute "time" \n 
 * @p state the state of the scheduler, 1 is running and 0 is idle \n 
 * @p running the id of the worker which is currently running, -1 if none of them is running \n 
 * @p ums_worker_list list of workers \n 
 */
typedef struct sched_item
{
        //info
        unsigned long id;
        int worker_num;
        struct task_struct* task_struct;
        //proc fs
        struct proc_dir_entry *dir;
        struct proc_dir_entry *workers;
        struct proc_dir_entry *info;
        //data for sched/info
        unsigned long counter;
        unsigned long total_time;
        unsigned long time;
        unsigned long last_time;
        int state;
        unsigned long running;
        //workers
        struct list_head ums_worker_list;
        rwlock_t worker_list_lock;
        struct list_head list;
} sched_item;

/**
 * @p tgid the tgid of the process \n 
 * @p num_sched number schedulers this process is managing \n 
 * @p ums_thread_list list of the workers of this process \n 
 * @p ums_sched_list list of the schedulers of this process \n 
 * @p proc_dir pointer to the /proc/pid directory \n 
 * @p sched_dir pointer to the proc/pid/sched/ directory \n 
 */
typedef struct ums_process
{
    int tgid;
    int num_sched;
    rwlock_t counter_lock;
    struct list_head ums_thread_list;
    rwlock_t sched_list_lock;
    struct list_head ums_sched_list;
    rwlock_t thread_list_lock;
    spinlock_t choice_lock;
    unsigned long flags;
    struct proc_dir_entry *proc_dir;
    struct proc_dir_entry *sched_dir;
    struct list_head list;
}ums_process;

