/**
 * @file UMSHeader.h
 * @brief User header
 * 
 * This header is intended to be distributed to the user, it contains all the prototypes and definitions needed to
 * interact with the UMS infrastructres.
 */

//we don't need doxygen to rewrite all the functions
#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <pthread.h>

typedef pthread_t ums_t;

struct completion_list_item{
    struct completion_list_item* next;
    struct completion_list_item* prev;
    ums_t ums_id;
    int prio;
};

struct completion_list{
    struct completion_list_item* head;
    struct completion_list_item* tail;
    int len;
};



struct completion_list* completion_list_create();
void completion_list_delete(struct completion_list*);
void completion_list_add(struct completion_list*, ums_t, int);
struct completion_list* DequeueUmsCompletionListItems(struct completion_list*);

//int UMS_init(void);
//void UMS_exit(void);

//user interface
ums_t EnterUmsSchedulingMode(void*, void *(*start_routine) (struct completion_list *, void *), void* );
ums_t EnterUmsWorkingMode(void *(*start_routine) (void *), void* );
void ExecuteUmsThread(ums_t);
void UmsThreadYield(void);
int ums_thread_join(ums_t thread, void **retval);
long unsigned ums_get_id(void);

#endif /*DOXYGEN_SHOULD_SKIP_THIS*/