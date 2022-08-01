/**
 * @file UMSmain.h
 * @brief Main kernel header
 * 
 * This header defines the core functions for the kernel implementation of this project. Once the module is loaded
 * a device file named /dev/ums-dev is created; this file is used to allow communication via IOCTL between
 * the kernel module and the library. 8 total requests are defined, they are listed as macros in this header.
 * The kernel module has been built and tested on linux kernel version 5.8.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>


#include "UMSProcManager.h"

#define DEVICE_NAME "ums-dev"
#define SUCCESS 0
#define UMS_ERROR -1


#define INIT_UMS_PROCESS            0
#define EXIT_UMS_PROCESS            1
#define INTRODUCE_UMS_TASK          3
#define INTRODUCE_UMS_SCHEDULER     4
#define EXECUTE_UMS_THREAD          5
#define UMS_THREAD_YIELD            6
#define UMS_WORKER_DONE             7
#define UMS_DEQUEUE                 8

#define MODULE_LOG "UMSmain: "





//macros, to optimize:
#define UMS_LIST_FIND_BY_ID(id, item, ums_thread_list)\
do{\
    read_lock_irqsave(&p->thread_list_lock, flags);\
    item = 0;\
    list_for_each(current_item_list, &ums_thread_list)\
    {\
        current_item = list_entry(current_item_list, thread_item, list);\
        if(current_item->id == id)\
            item = current_item;\
    }\
    read_unlock_irqrestore(&p->thread_list_lock, flags);\
}while(0)


#define UMS_LIST_FIND_SCHEDULER(sched, ums_thread_list)\
do{\
    read_lock_irqsave(&p->thread_list_lock, flags);\
    list_for_each(current_item_list, &ums_thread_list)\
    {\
        current_item = list_entry(current_item_list, thread_item, list);\
        if(current_item->task_struct == current)\
            sched = current_item->scheduler;\
    }\
    read_unlock_irqrestore(&p->thread_list_lock, flags);\
}while(0)


#define UMS_FIND_PROCESS_BY_TGID(pid, item)\
do{\
    ums_process* current_item;\
    struct list_head* current_item_list;\
    unsigned long flags;\
    read_lock_irqsave(&processes_list_lock, flags);\
    item = 0;\
    list_for_each(current_item_list, &ums_processes)\
    {\
        current_item = list_entry(current_item_list, ums_process, list);\
        if(current_item->tgid == pid)\
            item = current_item;\
    }\
    read_unlock_irqrestore(&processes_list_lock, flags);\
}while(0)


#define UMS_FIND_SCHED_ITEM(p, ts, item)\
do{\
    sched_item* current_item;\
    struct list_head* current_item_list;\
    unsigned long flags;\
    read_lock_irqsave(&p->sched_list_lock, flags);\
    item = 0;\
    list_for_each(current_item_list, &p->ums_sched_list)\
    {\
        current_item = list_entry(current_item_list, sched_item, list);\
        if(current_item->task_struct == ts)\
            item = current_item;\
    }\
    read_unlock_irqrestore(&p->sched_list_lock, flags);\
}while(0)


#define FIND_WORKER_BY_UMS_ID(s, id, item)\
do{\
    worker_info* current_item;\
    struct list_head* current_item_list;\
    unsigned long flags;\
    read_lock_irqsave(&s->worker_list_lock, flags);\
    item = 0;\
    list_for_each(current_item_list, &s->ums_worker_list)\
    {\
        current_item = list_entry(current_item_list, worker_info, list);\
        if(current_item->ums_id == id)\
            item = current_item;\
    }\
    read_unlock_irqrestore(&s->worker_list_lock, flags);\
}while(0)

//functions
int __init ums_init(void);
void __exit ums_exit(void);
long device_ioctl(struct file *, unsigned int, unsigned long);
void put_task_to_sleep(void);
int ums_schedule(unsigned long);
int ums_thread_yield(void);
int ums_thread_end(void);

int ums_create_worker_list(sched_item*, unsigned long);
void free_sched_list(ums_process*);
void free_work_list(ums_process*);

//ioctl management
void exit_ums_process(int);
void init_ums_process(int);
int new_task_management(unsigned long);
int new_scheduler_management(unsigned long);
int ums_dequeue_list(unsigned long);
void exit_ums_process_all(void);

//list operations
void ums_print_list(void);
thread_item* ums_list_find_by_id(unsigned long);
struct task_struct* ums_list_find_scheduler(void);

//aux
sched_item* ums_find_sched(ums_process*, struct task_struct*);
worker_info* find_worker_by_ums_id(sched_item*, unsigned long);

