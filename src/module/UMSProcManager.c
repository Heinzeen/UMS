#include "UMSProcManager.h"


char* root_name = "ums";
struct proc_dir_entry *root;

static struct proc_ops pops_sched =
    {
        .proc_read = myproc_read_sched,
        .proc_write = myproc_write,
};
static struct proc_ops pops_work =
    {
        .proc_read = myproc_read_work,
        .proc_write = myproc_write,
};

struct list_head* processes;
rwlock_t* list_lock;


ssize_t myproc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
        printk(KERN_ALERT MODULE_LOG "Sorry, this operation is not supported.\n");
        return -EINVAL;
}


/**
 * @p file the file in which we are trying to read \n 
 * @p ubuf the user buf in which we have to write the answer \n 
 * @p count the length of the read \n 
 * @p ppos the offset \n 
 * 
 * This function implements the read functionality for the files in path
 * /proc/ums/<pid>/schedulers/<sched_id>/workers/<worker_id>. The printed values are: \n 
 * @p ID the id of the worker (from 0 to n-1, where n is the number of workers in that thread) \n 
 * @p thread_ID the thread ID of that worker \n 
 * @p state the state of the scheduler, 0 means that it is running, 1 means that it is not \n 
 * @p  number_of_switches the number of switches
 */
ssize_t myproc_read_work(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
        char buf_path[PATH_MAX_LEN];
        char* path = dentry_path_raw(file->f_path.dentry, buf_path, PATH_MAX_LEN);
        long id, worker, pid  = aux_pid_from_path(path);
        int len = 0, estimated_len;
        char *buf;
        worker_info* w;
        sched_item* s;
        ums_process *p;

        PROC_FIND_PROCESS_BY_TGID((int) pid, p);
        id = aux_id_from_path(path);
        PROC_FIND_SCHED(p, id, s);
        estimated_len = s->worker_num * MAX_ENTRY_LEN + MAX_STAT_MSG_LEN;

        buf = kmalloc(MAX_WORK_INFO_LEN, GFP_KERNEL);
        worker = aux_worker_from_path(path);
        PROC_FIND_WORKER(s, worker, w);
        if (*ppos > 0 || count < estimated_len){
                kfree(buf);
                return 0;
        }
        len += sprintf(buf, "worker id: %d\nthread id: %lu\nstate: %d\nnumber of swithces: %d\n", w->id, w->ums_id, w->state, w->counter);

        if (copy_to_user(ubuf, buf, len)){
                kfree(buf);
                return -EFAULT;
        }
        *ppos = len;
        kfree(buf);

        return len;
}

/**
 * @p file the file in which we are trying to read \n 
 * @p ubuf the user buf in which we have to write the answer \n 
 * @p count the length of the read \n 
 * @p ppos the offset \n 
 * 
 * This function implements the read functionality for the files in path
 * /proc/ums/<pid>/schedulers/<sched_id>/info. The printed values are: \n 
 * @p ID the id of the scheduler (from 0 to n-1, where n is the number of schedulers) \n 
 * @p switches the number of switches done during the whole execution \n 
 * @p state the state of the scheduler, 0 means that it is running some worker, 1 means that it is not \n 
 * @p running the id id of the running thread \n 
 * @p last_switch_time how much time (in ns) did it take to do the last switch \n 
 * @p avg_switch_time the average time needed to do the switches \n 
 * @p completion_list the list of thread with their IDs
 */
ssize_t myproc_read_sched(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
        char buf_path[PATH_MAX_LEN];
        char* path = dentry_path_raw(file->f_path.dentry, buf_path, PATH_MAX_LEN);
        long id, pid  = aux_pid_from_path(path);
        int len = 0, i, estimated_len;
        char entry[MAX_ENTRY_LEN], *buf;
        worker_info* w;
        sched_item* s;
        ums_process *p;

        PROC_FIND_PROCESS_BY_TGID((int) pid, p);
        id = aux_id_from_path(path);
        PROC_FIND_SCHED(p, id, s);
        estimated_len = s->worker_num * MAX_ENTRY_LEN + MAX_STAT_MSG_LEN;

        //printk(KERN_DEBUG MODULE_LOG "Trying to retrieve info of a scheduler");
        buf = kmalloc(estimated_len, GFP_KERNEL);
        if (*ppos > 0 || count < estimated_len){
                kfree(buf);
                return 0;
        }
        len += sprintf(buf, "ID: %ld\nswitches: %lu\nstate: %d\nrunning: %ld\nlast switch time[ns]: %ld\navg switch time[ns]: %ld\n",
                                                s->id, s->counter, s->state, s->running, s->time, s->total_time/s->counter);
                                                
        i = 0;
        PROC_FIND_WORKER(s, i, w);
        while(w){
                i++;
                len += sprintf(entry, "worker id: %d, ums_id: %ld\n", w->id, w->ums_id);
                strcat(buf, entry);
                PROC_FIND_WORKER(s, i, w);
        }

        if (copy_to_user(ubuf, buf, len)){
                kfree(buf);
                return -EFAULT;
        }
        *ppos = len;
        kfree(buf);

        return len;
}

/**
 * @p ums_processes the list of processes that are currently using UMS
 * 
 * This function initializes the /proc/ums directory.
 */
void ums_create_proc_root(struct list_head* ums_processes, rwlock_t* lock){

        processes = ums_processes;
        list_lock = lock;

        root = proc_mkdir(root_name,NULL);

}

/**
 * @fn ums_delete_proc_root
 * 
 * Delete the /proc/ums directory.
 */
void ums_delete_proc_root(void){

        proc_remove(root);

}

/**
 * @p p process that is issuing the request
 * 
 * Creates the subtree proc/ums/pid for the requesting process.
 */
void ums_create_proc_process(ums_process* p){
        int tgid = current->tgid;
        char buf[9];        

        sprintf(buf, "%d", tgid);
        buf[8] = 0;

        p->proc_dir = proc_mkdir(buf,root);
        p->sched_dir = proc_mkdir("schedulers",p->proc_dir);

}

/**
 * @p p process that is issuing the request
 * 
 * Deletes the subtree /proc/ums/pid
 */
void ums_delete_proc_process(ums_process* p){
    
        proc_remove(p->proc_dir);

}

/**
 * @p p process that is issuing the request \n 
 * @p s scheduler that is issuing the request \n 
 * 
 * Creates the entries for the scheduler in the /proc fs.
 */
void ums_create_proc_sched(ums_process* p, sched_item* s){
        char buf[9];        

        sprintf(buf, "%ld", s->id);
        buf[8] = 0;

        s->dir = proc_mkdir(buf, p->sched_dir);
        s->workers = proc_mkdir("workers", s->dir);   
        proc_create("info", S_IALLUGO, s->dir, &pops_sched);
}

/**
 * @p s scheduler that is issuing the request \n 
 * @p id id of the worker thread that is issuing the request \n 
 * 
 * Creates the entries for the worker in the /proc fs.
 */
void ums_create_proc_worker(sched_item* s, int id){
        char buf[9];        
        sprintf(buf, "%d", id);
        buf[8] = 0;
        proc_create(buf, S_IALLUGO, s->workers, &pops_work);

}


//auxiliary methods, used to manage the strings:

//return pid (or tgid)
long aux_pid_from_path(char* path){
        int i;
        char buf[MAX_NUM_LEN];
        long ret;
        for(i=0; i<MAX_NUM_LEN - 1; i++){
                if(path[UMS_PREFIX_LEN + i] == '/')
                        break;
                buf[i] = path[UMS_PREFIX_LEN + i];
        }
        buf[i] = 0;
        kstrtol(buf, 10, &ret);         //base 10
        return ret;
}

long aux_id_from_path(char* path){
        int i, j, counter = 0;
        char buf[MAX_NUM_LEN];
        long ret;
        for(i=0; path[i]; i++){
                if(path[i] == '/')
                        counter++;
                if(counter == NUMBER_OF_SLASHES_BEFORE_ID)
                        break;
        }
        i++;
        for(j=0; path[i+j]; j++){
                if(path[i+j] == '/')
                        break;
                buf[j] = path[i+j];
        }

        buf[j] = 0;
        kstrtol(buf, 10, &ret);         //base 10
        return ret;
}

long aux_worker_from_path(char* path){
        int i, j, counter = 0;
        char buf[32];
        long ret;
        for(i=0; path[i]; i++){
                if(path[i] == '/')
                        counter++;
                if(counter == NUMBER_OF_SLASHES_BEFORE_WORKER)
                        break;
        }
        i++;
        for(j=0; path[i+j]; j++){
                if(path[i+j] == 0 )
                        break;
                buf[j] = path[i+j];
        }


        buf[j] = 0;
        kstrtol(buf, 10, &ret);         //base 10

        return ret;
}

char * strcat(char *dest, const char *src)
{
    int i, j;
    for (i = 0; dest[i] != 0; i++)
        ;

    for (j = 0; src[j] != 0; j++)
        dest[j + i] = src[j];
        
    dest[j + i] = 0;
    return dest;
}
