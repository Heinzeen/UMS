#include "UMSmain.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matteo Marini <matteo.marini4898@gmail.com>");
MODULE_DESCRIPTION("Kernel module to implement user mode scheduling");
MODULE_VERSION("1.0.7");


//create the list of all processes
struct list_head ums_processes;
//use locks to access the list shared by all processes
rwlock_t processes_list_lock = __RW_LOCK_UNLOCKED(processes_list_lock);

//declaration of the properties of the device file
static struct file_operations fops = {
    .unlocked_ioctl = device_ioctl
};

static struct miscdevice mdev = {
    .minor = 0,
    .name = DEVICE_NAME,
    .mode = S_IALLUGO,
    .fops = &fops
};

//init and exit of the module
module_init(ums_init);
module_exit(ums_exit);

/**
 * @fn ums_init()
 * This function initializes the kernel module; it also initializes the device file needed for the communication
 * with user space.
 */
int __init ums_init(void)
{
    int ret;
    printk(KERN_INFO MODULE_LOG "Module init.\n");

    ret = misc_register(&mdev);

    if (ret < 0)
    {
        printk(KERN_ALERT MODULE_LOG "Registering char device failed\n");
        return ret;
    }
    printk(KERN_DEBUG MODULE_LOG "Device registered successfully\n");

    INIT_LIST_HEAD(&ums_processes);

    ums_create_proc_root(&ums_processes, &processes_list_lock);

    return SUCCESS;

}

/**
 * @fn ums_exit()
 * This function exits the kernel module, it removes the device file and it removes the directory /proc/ums.
 * Moreover, if some memory is left allocated, it is freed.
 */
void __exit ums_exit(void)
{
    misc_deregister(&mdev);

    ums_delete_proc_root();

    exit_ums_process_all();

    printk(KERN_INFO MODULE_LOG "Module done, exiting\n");
}

/**
 * 
 * @p file the file from which IOCTL was issued \n 
 * @p request the request issued \n 
 * @p data the data passed from the user, if any \n 
 * 
 * This function manages the requests done with IOCTL to the kernel module. the value of @p request
 * is analyzed, and for each request the correct handler is called
 */
long device_ioctl(struct file *file, unsigned int request, unsigned long data)
{
    int ret;
    //printk(KERN_DEBUG MODULE_LOG "Device_ioctl: pid->%d, path=%s, request=%u\n", current->pid, file->f_path.dentry->d_iname, request);

    switch(request){
        case INIT_UMS_PROCESS:
            printk(KERN_INFO MODULE_LOG "Process %d is initializing UMS\n", current->pid);
            init_ums_process(current->pid);
            ret = SUCCESS;
            break;

        case EXIT_UMS_PROCESS:
            printk(KERN_INFO MODULE_LOG "Process %d is exiting UMS\n", current->pid);
            exit_ums_process(current->pid);
            ret = SUCCESS;
            break;

        case INTRODUCE_UMS_TASK:
            //printk(KERN_INFO MODULE_LOG "New worker thread created\n");
            ret = new_task_management(data);
            break;

        case INTRODUCE_UMS_SCHEDULER:
            //printk(KERN_INFO MODULE_LOG "New scheduler created\n");
            ret = new_scheduler_management(data);
            break;

        case EXECUTE_UMS_THREAD:
            //printk(KERN_INFO MODULE_LOG "Scheduler wants to execute a new thread\n");
            ret = ums_schedule(data);
            break;

        case UMS_THREAD_YIELD:
            //printk(KERN_INFO MODULE_LOG "thread %d wants to sleep\n", current->pid);
            ret = ums_thread_yield();
            break;

        case UMS_WORKER_DONE:
            //printk(KERN_INFO MODULE_LOG "thread %d ending\n", current->pid);
            ret = ums_thread_end();
            break;

        case UMS_DEQUEUE:
            //printk(KERN_INFO MODULE_LOG "Dequeue ums request\n");
            ret = ums_dequeue_list(data);
            break;
        
        default:
            printk(KERN_INFO MODULE_LOG "Received IOCTL with unknown request ID\n");
            ret = UMS_ERROR;
    }

    return ret;
}

/**
 * @fn ums_thread_end
 * 
 * This function is called when a worker thread ends; it cleans that worker's memory and wakes up its (last) scheduler.
 */
int ums_thread_end(){
    thread_item *tmp;
    struct list_head* current_item_list, *q;
    struct task_struct* sched = 0;
    unsigned long flags;
    ums_process *p;

    UMS_FIND_PROCESS_BY_TGID(current->tgid, p);

    if(!p){
        printk(KERN_ALERT MODULE_LOG "Could not retrieve a thread's process, aborting ums_thread_end\n");
        return UMS_ERROR;
    }

    write_lock_irqsave(&p->thread_list_lock, flags);
    list_for_each_safe(current_item_list, q, &p->ums_thread_list)
    {
        //printk(KERN_INFO MODULE_LOG "Iterating, current = %p, item=%p", current, tmp->task_struct);
        tmp = list_entry(current_item_list, thread_item, list);
        if(tmp->task_struct == current){
            sched = tmp->scheduler;
            //printk(KERN_INFO MODULE_LOG "Removing current_item=%p, current_item->id=%ld, calling sched: %p\n", tmp, tmp->id, sched);
            list_del(current_item_list);
            kfree(tmp);
            break;
        }
    }
    write_unlock_irqrestore(&p->thread_list_lock, flags);

    if(!sched){
        printk(KERN_ALERT MODULE_LOG "Could not retrieve a thread's scheduler, aborting ums_thread_end\n");
        return UMS_ERROR;
    }
    
    while(!wake_up_process(sched)){}

    return SUCCESS;
}

/**
 * @p data containts the pointer to the id of the thread that needs to be scheduled
 * 
 * This function schedules the next thread to be executed by a scheduler. The scheduling is done by
 * putting the scheduler to sleep in TASK_INTERRUPTIBLE and then by waking up the selected thread. 
 */
int ums_schedule(unsigned long data){
    unsigned long id, flags;
    thread_item* next;
    sched_item* s;
    worker_info* w;

    //to search the new thread with the macro
    thread_item* current_item;
    struct list_head* current_item_list;
    ums_process *p;


    UMS_FIND_PROCESS_BY_TGID(current->tgid, p);
    if(!p){
        printk(KERN_ALERT MODULE_LOG "Could not retrieve a thread's process, aborting ums_schedule\n");
        return UMS_ERROR;
    }
    if(!data){
        printk(KERN_ALERT MODULE_LOG "NULL pointer found in ums_scheduler request!\n");
        return UMS_ERROR;
    }

    copy_from_user(&id, (unsigned long*) data, sizeof(id));
    UMS_FIND_SCHED_ITEM(p, current, s);
    if(next)
        FIND_WORKER_BY_UMS_ID(s, next->id, w);

    UMS_LIST_FIND_BY_ID(id, next, p->ums_thread_list);

    spin_lock_irqsave(&p->choice_lock, flags);
    if(next && next->task_struct->state == TASK_INTERRUPTIBLE){

        //remember who is the scheduler
        next->scheduler = current;
        UMS_FIND_SCHED_ITEM(p, current, s);
        FIND_WORKER_BY_UMS_ID(s, next->id, w);

        if(w){
            w->state = 1;
            w->counter = w->counter + 1;
        }
        if(s){
            s->counter++;
            s->state = 0;
            s->running = next->id;
            s->last_time = ktime_get_ns();
        }
        while(!wake_up_process(next->task_struct)){}
        spin_unlock_irqrestore(&p->choice_lock, flags);

        put_task_to_sleep();

        //here the scheduler is executed after the thread yeilded again
        if(s){
            s->state = 1;
            s->running = -1;
        }
        if(w)
            w->state = 0;
    }
    else    //in case we don't run it, we still need to release the lock (otherwise, deadlock incoming)
        spin_unlock_irqrestore(&p->choice_lock, flags);
    
    return SUCCESS;
}

/**
 * @fn ums_thread_yield
 * 
 * Called from a worker thread, it gives the control back to its (last) scheduler that will decide the
 * next worker to be run. This is done by putting the thread in TASK_INTERRUPTIBLE state.
 */

int ums_thread_yield(){
    thread_item* current_item;
    struct list_head* current_item_list;
    unsigned long flags;
    sched_item* s;
    struct task_struct* sched;
    ums_process *p;

    UMS_FIND_PROCESS_BY_TGID(current->tgid, p);
    if(!p){
        printk(KERN_ALERT MODULE_LOG "Could not retrieve a thread's process, aborting ums_thread_yield\n");
        return UMS_ERROR;
    }

    UMS_LIST_FIND_SCHEDULER(sched, p->ums_thread_list);

    while(!wake_up_process(sched)){}

    put_task_to_sleep();

    //after we are scheduled, we restart from here
    UMS_FIND_SCHED_ITEM(p, sched, s);
    //this is less severe, if we don't find the scheduler we just have problems in updating the time
    if(!s){
        printk(KERN_WARNING MODULE_LOG "Could not retrieve a thread's scheduler\n");
        return UMS_ERROR;
    }
    s->time = ktime_get_ns() - s->last_time;
    s->total_time += s->time;

    return SUCCESS;
}

void put_task_to_sleep(){

    //printk(KERN_INFO MODULE_LOG "Putting to sleep %d\n", current->pid);

    set_current_state(TASK_INTERRUPTIBLE);
    schedule();

}

/**
 * @p data the pointer to the id of the new thread
 * 
 * Called from a worker thread, this function initializes all the data needed to manage a 
 * new worker thread.
 */

int new_task_management(unsigned long data){
    unsigned long pthread_id;
    ums_process *p;
    //thread_item* temp;
    thread_item* item = kmalloc(sizeof(thread_item), GFP_KERNEL);
    unsigned long flags;

    UMS_FIND_PROCESS_BY_TGID(current->tgid, p);
    if(!p){
        printk(KERN_ALERT MODULE_LOG "Could not retrieve a thread's process, aborting new_task_management\n");
        return UMS_ERROR;
    }

    if(!data){
        printk(KERN_ALERT MODULE_LOG "NULL pointer found in new_task_management request!\n");
        return UMS_ERROR;
    }

    copy_from_user(&pthread_id, (unsigned long*) data, sizeof(pthread_id));

    item = kmalloc(sizeof(thread_item), GFP_KERNEL);
    item->id = pthread_id;
    item->task_struct = current;
    write_lock_irqsave(&p->thread_list_lock, flags);
    list_add(&item->list, &p->ums_thread_list);
    write_unlock_irqrestore(&p->thread_list_lock, flags);

    //printk(KERN_INFO MODULE_LOG "New worker thread created, ts = %p, pthread_id = %lu\n", current, pthread_id);

    put_task_to_sleep();

    return SUCCESS;

}

int ums_create_worker_list(sched_item* s, unsigned long ptr){
    unsigned long len, *mem;
    unsigned long i, id;
    unsigned long flags;
    worker_info* w;

    //init the lock
    s->worker_list_lock = __RW_LOCK_UNLOCKED(s->worker_list_lock);
    INIT_LIST_HEAD(&s->ums_worker_list);

    copy_from_user(&len, (unsigned long*) ptr, sizeof(len));
    if(len <= 0){
        printk(KERN_ALERT MODULE_LOG "Bad len found in completion_list->len\n");
        return UMS_ERROR;
    }

    mem = kmalloc(len * sizeof(unsigned long), GFP_KERNEL);
    copy_from_user(mem, (unsigned long*) (ptr + sizeof(unsigned long)), len * sizeof(unsigned long));

    s->worker_num = len;
    
    for(i=0; i<len; i++){
        id = mem[i];
        w = kmalloc(sizeof(worker_info), GFP_KERNEL);
        w->id = i;
        w->ums_id = id;
        w->state = 0;
        w->counter = 0;
        
        write_lock_irqsave(&s->worker_list_lock, flags);
        list_add(&w->list, &s->ums_worker_list);
        write_unlock_irqrestore(&s->worker_list_lock, flags);
        ums_create_proc_worker(s, i);
        //printk(KERN_INFO MODULE_LOG "sched %p :Creating worker, id = %d, ums_id=%lu\n", s, w->id, w->ums_id);
    }

    kfree(mem);

    return SUCCESS;
}

/**
 * @p ptr the pointer to the id of the new scheduler thread
 * 
 * Called from a scheduler thread, this function initializes all the data needed to manage a 
 * new scheduler thread. To understand how the list is read, refere to the function ums_dequeue_list() since they use
 * the same mechanism.
 */
int new_scheduler_management(unsigned long ptr){
    unsigned long flags;
    sched_item* item = kmalloc(sizeof(sched_item), GFP_KERNEL);
    ums_process *p;

    UMS_FIND_PROCESS_BY_TGID(current->tgid, p);

    if(!p){
        printk(KERN_ALERT MODULE_LOG "Could not retrieve a thread's process, aborting new_scheduler_management\n");
        return UMS_ERROR;
    }
    if(!ptr){
        printk(KERN_ALERT MODULE_LOG "NULL pointer found in new_scheduler_management request!\n");
        return UMS_ERROR;
    }

    //take the id, we have to use a lock
    write_lock_irqsave(&p->counter_lock, flags);
    item->id = p->num_sched;
    p->num_sched = p->num_sched + 1;
    write_unlock_irqrestore(&p->counter_lock, flags);

    item->task_struct = current;
    item->counter = 0;
    item->time = 0;
    item->state = 1;
    item->running = -1;
    item->worker_num = 0;   //it will change in next functions
    //create the proc fs entries
    ums_create_proc_sched(p, item);

    if(ums_create_worker_list(item, ptr))
        return UMS_ERROR;

    //create an entry in the scheduler list
    write_lock_irqsave(&p->sched_list_lock, flags);
    list_add(&item->list, &p->ums_sched_list);
    write_unlock_irqrestore(&p->sched_list_lock, flags);

    //printk(KERN_INFO MODULE_LOG "New scheduler, task_struct = %p, id=%ld, item=%p\n", current, item->id, item);

    return SUCCESS;
}


/**
 * @p ptr pointer to the memory in which the user saved the completion list
 * 
 * This function returns the ready threads that can be executed in the completion list
 * of the calling scheduler thread. This is done by reading first the first 8 byte (sizeof(unsigned long))
 * of the memory pointed by @p ptr (passed with IOCTL) to read the length of the list, then a new copy_from_user()
 * is issued to read the list (now we know the dimension). This mechanism was used in order to avoid using 2
 * different IOCTLs (one to read the length, and a second one to read the list). The same mechanism is used in function
 * new_scheduler_management().
 */
int ums_dequeue_list(unsigned long ptr){
    unsigned long len;
    unsigned long *mem;
    unsigned long id;
    unsigned long flags;
    int i, found = 0, exist = 1;
    struct thread_item* aux;
    ums_process *p;
    //to search the new thread with the macro
    thread_item* current_item;
    struct list_head* current_item_list;

    UMS_FIND_PROCESS_BY_TGID(current->tgid, p);

    if(!p){
        printk(KERN_ALERT MODULE_LOG "Could not retrieve a thread's process, aborting ums_dequeue_list\n");
        return UMS_ERROR;
    }
    if(!ptr){
        printk(KERN_ALERT MODULE_LOG "NULL pointer found in ums_dequeue_list request!\n");
        return UMS_ERROR;
    }

    copy_from_user(&len, (unsigned long*) ptr, sizeof(len));
    if(len <= 0){
        printk(KERN_ALERT MODULE_LOG "Bad len found in completion_list->len, aborting ums_dequeue_list\n");
        return UMS_ERROR;
    }
    mem = kmalloc(len * sizeof(unsigned long), GFP_KERNEL);
    copy_from_user(mem, (unsigned long*) (ptr + sizeof(unsigned long)), len * sizeof(unsigned long));

    //printk(KERN_INFO MODULE_LOG "number of items = %lu, 1st=%lu 2nd=%lu\n", len, mem[0], mem[1]);

    //if no thread is ready, iterate
    while(!found && exist){
        found = 0;
        exist = 0;
        for(i=0; i<len; i++){
            id = mem[i];
            UMS_LIST_FIND_BY_ID(id, aux, p->ums_thread_list);
            if(!aux)                //the item does not exist yet, or it has exited alreay
                mem[i] = 0;
            else{
                if(aux->task_struct->state != TASK_INTERRUPTIBLE){
                    mem[i] = 0;
                }
                found = 1;
                exist = 1;
            }
        }
    }

    if(!found){
        //printk(KERN_INFO MODULE_LOG "No thread found to be executed!");
    }

    copy_to_user((unsigned long*) (ptr + sizeof(unsigned long)), mem, len * sizeof(unsigned long));

    kfree(mem);
    
    return SUCCESS;
}

void free_sched_list(ums_process* p){

    struct list_head* current_sched, *s;
    struct list_head* current_worker, *w;
    sched_item * t;
    worker_info * i;
    unsigned long flags, flags1;

    write_lock_irqsave(&p->sched_list_lock, flags);

    //free every sched_item
    list_for_each_safe(current_sched, s, &p->ums_sched_list)
    {
        t = list_entry(current_sched, sched_item, list);

        //free every worker_info
        write_lock_irqsave(&t->worker_list_lock, flags1);
        list_for_each_safe(current_worker, w, &t->ums_worker_list)
        {
            i = list_entry(current_worker, worker_info, list);
            list_del(current_worker);
            //printk(KERN_INFO MODULE_LOG "Freeing worker, id = %d, ums_id=%ld\n", i->id, i->ums_id);
            kfree(i);
        }
        write_unlock_irqrestore(&t->worker_list_lock, flags1);

        list_del(current_sched);
        //printk(KERN_INFO MODULE_LOG "Freeing scheduler, task_struct = %p, id=%ld\n", t->task_struct, t->id);
        kfree(t);
    }
    write_unlock_irqrestore(&p->sched_list_lock, flags);
}

void free_work_list(ums_process* p){

    thread_item *tmp;
    struct list_head* current_item_list, *q;
    struct task_struct* sched = 0;
    unsigned long flags;

    write_lock_irqsave(&p->thread_list_lock, flags);
    list_for_each_safe(current_item_list, q, &p->ums_thread_list)
    {
        //printk(KERN_INFO MODULE_LOG "Iterating, current = %p, item=%p", current, tmp->task_struct);
        tmp = list_entry(current_item_list, thread_item, list);
        if(tmp->task_struct == current){
            sched = tmp->scheduler;
            //printk(KERN_INFO MODULE_LOG "Removing current_item=%p, current_item->id=%ld, calling sched: %p\n", tmp, tmp->id, sched);
            list_del(current_item_list);
            kfree(tmp);
            break;
        }
    }
    write_unlock_irqrestore(&p->thread_list_lock, flags);
}

void exit_ums_process_all(){
    ums_process* p;
    struct list_head* current_item_list, *q;
    unsigned long flags;

    write_lock_irqsave(&processes_list_lock, flags);

    //free the ums_process 
    list_for_each_safe(current_item_list, q, &ums_processes)
    {
        p = list_entry(current_item_list, ums_process, list);
            //delete /proc/pid/
            ums_delete_proc_process(p);

            free_sched_list(p);
            free_work_list(p);

            //remove from list
            list_del(current_item_list);
            kfree(p);
    }

    write_unlock_irqrestore(&processes_list_lock, flags);
}

/**
 * @p pid identifier of the process that is exiting UMS. We refer to "pid" in the user-space meaning of the term (i.e. tgid in kernel-space)
 * 
 * This function clears the memory used by a user-space process when using UMS.
 */
void exit_ums_process(int pid){
    ums_process* p;
    struct list_head* current_item_list, *q;
    unsigned long flags;

    write_lock_irqsave(&processes_list_lock, flags);

    //free the ums_process 
    list_for_each_safe(current_item_list, q, &ums_processes)
    {
        p = list_entry(current_item_list, ums_process, list);
        if(p->tgid == pid){
            //delete /proc/pid/
            ums_delete_proc_process(p);

            free_sched_list(p);
            free_work_list(p);

            //remove from list
            list_del(current_item_list);
            kfree(p);
        }
    }

    write_unlock_irqrestore(&processes_list_lock, flags);

}

/**
 * @p pid identifier of the process that is entring UMS. We refer to "pid" in the user-space meaning of the term (i.e. tgid in kernel-space)
 * 
 * This function initializes all the memory needed by a user space process to use UMS.
 */
void init_ums_process(int pid){
    ums_process* p = kmalloc(sizeof(ums_process), GFP_KERNEL);
    unsigned long flags;

    INIT_LIST_HEAD(&p->ums_sched_list);
    INIT_LIST_HEAD(&p->ums_thread_list);
    p->thread_list_lock = __RW_LOCK_UNLOCKED(p->thread_list_lock);
    p->sched_list_lock = __RW_LOCK_UNLOCKED(p->sched_list_lock);
    p->counter_lock = __RW_LOCK_UNLOCKED(p->counter_lock);
    p->choice_lock = __SPIN_LOCK_UNLOCKED(p->choice_lock);
    p->tgid = pid;
    p->num_sched = 0;

    //create an entry in the processes list
    write_lock_irqsave(&processes_list_lock, flags);
    list_add(&p->list, &ums_processes);
    write_unlock_irqrestore(&processes_list_lock, flags);

    ums_create_proc_process(p);

}
