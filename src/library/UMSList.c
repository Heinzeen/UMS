#include "UMSList.h"


/**
 * @fn completion_list_create
 * 
 * Created an empty completion list. The completion list has to be deleted using the function completion_list_delete()
 * in order to avoid memory leaks. If more than a scheduler is using the same completion list, be carefull not to
 * delete it before every scheduler is done, otherwise it will lead to unpredictable behaviours of the program.
 * Accesses to the list are controlled by semaphores.
 */
completion_list* completion_list_create(){
    int ret;

    //printf("Initializing completion list!\n");

    completion_list* cs = (completion_list*) malloc(sizeof(completion_list));
    cs->head = NULL;
    cs->tail = NULL;
    cs->len = 0;

    ret = sem_init(&(cs->sem), 0, 1);
    if(ret == -1){
        printf("Could not initialize the semaphore Aborting");
        exit(-3);
    }

    return cs;
}

/**
 * @p cs the list to be deleted
 * Deletes the list and all its elements. The use of a list after this function was called on it will lead to
 * unpredictable results (in general, use after free problems).
 */
void completion_list_delete(completion_list* cs){
    int ret;

    //printf("Removing completion list!\n");

    completion_list_item* item = cs->head;
    completion_list_item* aux;

    while(item){
        //printf("Freeing item %p\n", item);
        aux = item->next;
        free(item);
        item = aux;
    }

    ret = sem_destroy(&(cs->sem));
    if(ret == -1){
        printf("Could not remove the semaphore, Aborting");
        exit(-3);
    }

    free(cs);
}

void completion_list_append(completion_list* cs, completion_list_item* item){

    sem_wait(&cs->sem);

    //first item
    if(cs->head == NULL){
        fflush(stdout);
        cs->head = item;
        cs->tail = item;
        item->next = NULL;
        item->prev = NULL;
    }

    else{
        cs->tail->next = item;
        item->prev = cs->tail;
        item->next = NULL;
        cs->tail = item;

    }

    cs->len += 1;

    sem_post(&cs->sem);

}

/**
 * @p cs the completion list on which the add has to be performed
 * @p ums_id the id of the element that needs to be added
 * @p prio the priority of the element that needs to be added
 * 
 * Adds an element to the tail of the given completion list; uses the function completion_list_append which is not exposed.
 */
void completion_list_add(completion_list* cs, ums_t ums_id, int prio){

    completion_list_item* item = (completion_list_item*) malloc(sizeof(completion_list_item));

    item->ums_id = ums_id;
    item->prio = prio;

    completion_list_append(cs, item);

}




/**
 * @p cs the completion list that has to be printed
 * 
 * This function prints a completion list; the use of this function is intended only for debugging purposes,
 * it remains exposed in case a user needs to do some debugging checks.
 */
void completion_list_print(completion_list* cs){

    sem_wait(&cs->sem);

    printf("Printing completion list\n");
    printf("Head=0x%p tail=0x%p\n", cs->head, cs->tail);

    completion_list_item* item = cs->head;

    while(item){
        printf("New item at %p, ums_id=%ld, prio=%d\n", item, item->ums_id, item->prio);
        printf("with links to: prev=0x%p, next=0x%p\n\n", item->prev, item->next);
        item = item->next;
    }

    sem_post(&cs->sem);

}