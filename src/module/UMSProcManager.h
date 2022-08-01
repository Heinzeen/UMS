/**
 * @file UMSProcManager.h
 * @brief Header that manages the proc fs
 * 
 * This header defines the core functions used to manage the proc fs functionalities.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>

#include "common.h"

//remove
#define MODULE_LOG "UMSmain: "
#define BUFSIZE 256
#define PATH_MAX_LEN    128
#define MAX_ENTRY_LEN   64
#define MAX_STAT_MSG_LEN    256
#define MAX_WORK_INFO_LEN   128
#define MAX_NUM_LEN     32
#define UMS_PREFIX_LEN  5       //strlen(/ums/)
#define NUMBER_OF_SLASHES_BEFORE_ID 4
#define NUMBER_OF_SLASHES_BEFORE_WORKER 6

#define PROC_FIND_PROCESS_BY_TGID(id, item)\
do{\
    ums_process* current_item;\
    struct list_head* current_item_list;\
    unsigned long flags;\
    read_lock_irqsave(list_lock, flags);\
    item = 0;\
    list_for_each(current_item_list, processes)\
    {\
        current_item = list_entry(current_item_list, ums_process, list);\
        if(current_item->tgid == id)\
            item = current_item;\
    }\
    read_unlock_irqrestore(list_lock, flags);\
}while(0)


#define PROC_FIND_SCHED(p, sched_id, item)\
do{\
    sched_item* current_item;\
    struct list_head* current_item_list;\
    unsigned long flags;\
    read_lock_irqsave(&p->sched_list_lock, flags);\
    item = 0;\
    list_for_each(current_item_list, &p->ums_sched_list)\
    {\
        current_item = list_entry(current_item_list, sched_item, list);\
        if(current_item->id == sched_id)\
            item = current_item;\
    }\
    read_unlock_irqrestore(&p->sched_list_lock, flags);\
}while(0)


#define PROC_FIND_WORKER(s, work_id, item)\
do{\
    worker_info* current_item;\
    struct list_head* current_item_list;\
    unsigned long flags;\
    read_lock_irqsave(&s->worker_list_lock, flags);\
    item = 0;\
    list_for_each(current_item_list, &s->ums_worker_list)\
    {\
        current_item = list_entry(current_item_list, worker_info, list);\
        if(current_item->id == work_id)\
            item = current_item;\
    }\
    read_unlock_irqrestore(&s->worker_list_lock, flags);\
}while(0)

//proc fs management
void ums_create_proc_root(struct list_head*, rwlock_t*);
void ums_delete_proc_root(void);
void ums_create_proc_process(ums_process*);
void ums_delete_proc_process(ums_process*);
void ums_create_proc_sched(ums_process*, sched_item*);
void ums_delete_proc_sched(sched_item*);
void ums_create_proc_worker(sched_item*, int);


ssize_t myproc_read_sched(struct file *file, char __user *ubuf, size_t count, loff_t *offset);
ssize_t myproc_read_work(struct file *file, char __user *ubuf, size_t count, loff_t *offset);
ssize_t myproc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *offset);

//aux functions
long aux_pid_from_path(char*);
long aux_id_from_path(char*);
long aux_worker_from_path(char*);
char * strcat(char *, const char *);
long aux_worker_from_path(char*);