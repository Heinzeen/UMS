#include <pthread.h>
#include <unistd.h>
#include "stdlib.h"
#include <stdio.h>
#include "../UMSHeader.h"

#define SCHED_ID        "[Sched #%ld]"
#define WORK_ID         "[Work #%lu]"

#define NUM_ITERATION   1000000   //one milion
#define NUM_CYCLES      4        //it is better to pick a divisor of NUM_ITERATION
#define NUM_SCHED       2
#define NUM_WORKER      2
#define TOTAL_THREADS   NUM_SCHED*(NUM_WORKER + 1)

// Global variable:

// Starting routine:
void* scheduler(struct completion_list* list, void* arg){
    struct completion_list_item * item, *next;
    struct completion_list* ready;
    int min_prio;

    long unsigned id = (long unsigned) ums_get_id() % 10000;
    printf(SCHED_ID "Scheduler initiated\n",id);

        while(1){
            ready = DequeueUmsCompletionListItems(list);

            if(ready->len == 0){            //in case we exit, we still need to free the list!!!
                completion_list_delete(ready);
                break;
            }

            item = ready->head;
            next = item;

            //compute the thread with higher priority (lower item->prio)
            min_prio = item->prio;
            while(item){
                if(item->prio < min_prio){
                    min_prio = item->prio;
                    next = item;
                }
                item = item->next;
            }
            printf(SCHED_ID "Execute %ld with prio %d\n", id, next->ums_id % 10000, next->prio);
            ExecuteUmsThread(next->ums_id);

            completion_list_delete(ready);

        }
    printf(SCHED_ID "Scheduler exiting\n",id);

    return 0;

}


void* foo1(void* arg){
    int i, j;
    int* counter = (int *) arg;

    long unsigned id = (long unsigned) ums_get_id() % 10000;

    //printf(WORK_ID "Worker thread started! full id = %ld\n", id, ums_get_id());

    for(i=0; i< NUM_CYCLES; i++){
        for(j=0; j<NUM_ITERATION/NUM_CYCLES; j++){
            *counter = *counter + 1 ;
        }
    
        //printf(WORK_ID "Worker thread number %ld going to yield!\n\n", id, id);
        UmsThreadYield();
    }

    printf(WORK_ID "Worker thread number %lu going to finish!\n", id, id);

    return 0;

}


int main() {
    int i, j;
    ums_t id[NUM_SCHED*(NUM_WORKER + 1)];

    int vars[NUM_SCHED];
    struct completion_list* cs[NUM_SCHED];

    for(i=0; i<NUM_SCHED; i++){
        vars[i] = 0;

        cs[i] = completion_list_create();
        for(j=1; j<=NUM_WORKER; j++){
            id[j + i*(NUM_WORKER + 1)] = EnterUmsWorkingMode(foo1, &vars[i]);
            completion_list_add(cs[i], id[j + i*(NUM_WORKER + 1)], j);
        }
        id[i*(NUM_WORKER + 1)] = EnterUmsSchedulingMode(cs[i], scheduler, &vars[i]);
    }


    for(i=0; i<TOTAL_THREADS; i++)
        ums_thread_join(id[i], 0);

    for(i=0; i<NUM_SCHED; i++)
        completion_list_delete(cs[i]);

    printf("Main exiting, final value of the counters:\n");

    for(i=0; i<NUM_SCHED; i++){
        printf("[%d] %d\n", i, vars[i]);
    }

    char exit;
    printf("Press enter to exit [perhaps you can use this time to check /proc fs]: ");
    scanf("%c", &exit);
    printf("\n");

}