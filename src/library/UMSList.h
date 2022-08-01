/**
 * @file UMSList.h
 * @brief Manage the completion lists used by the user.
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

typedef pthread_t ums_t;

/**
 * @p next next item of the list \n 
 * @p prev previous item of the list \n 
 * @p ums_id ID of the thread \n 
 * @p prio the priority given by the user \n 
 */
typedef struct completion_list_item{
    struct completion_list_item* next;
    struct completion_list_item* prev;
    ums_t ums_id;
    int prio;
}completion_list_item;

/**
 * @p head first item of the list \n 
 * @p tail last item of the list \n 
 * @p len length of the list \n 
 * @p semaphore used to access the list \n 
 */
typedef struct completion_list{
    completion_list_item* head;
    completion_list_item* tail;
    int len;
    sem_t sem;
}completion_list;



completion_list* completion_list_create();
void completion_list_delete(completion_list*);
void completion_list_add(completion_list*, ums_t, int);

//debug only
void completion_list_print(completion_list*);