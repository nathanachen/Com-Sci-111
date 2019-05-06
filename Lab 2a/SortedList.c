#include "SortedList.h"
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>


void SortedList_insert(SortedList_t *list, SortedListElement_t *element)
{
    if ((opt_yield & INSERT_YIELD) != 0)
    {
        sched_yield();
    }
    SortedListElement_t *insertion = (SortedListElement_t *) malloc(sizeof(SortedListElement_t));
    if (insertion == NULL)
    {
        fprintf(stderr, "ERROR: Unable to use malloc \n");
        exit(1);
    }
    SortedListElement_t * temp = list;
    (*insertion).key = (*element).key;
    if (((*temp).next)->key == NULL)
    {
        insertion -> next = list;
        insertion -> next = list;
        list -> next = insertion;
        list -> prev = insertion;
        return;
    }
    else
    {
        const char * keyHold1 = (temp->next) -> key;
        const char * keyHold2 = insertion->key;
        while ((strcmp(keyHold1, keyHold2) <= 0) && ((temp->next)-> key != NULL))
        {
            temp = temp -> next;
        }
        SortedListElement_t * nextOne = temp->next;
        temp -> next = insertion;
        nextOne -> prev = insertion;
        insertion -> prev = temp;
        insertion -> next = nextOne;
    }
}

int SortedList_delete( SortedListElement_t *element)
{
    if ((opt_yield & DELETE_YIELD) != 0)
    {
        sched_yield();
    }
    if (element -> key == NULL)
    {
        fprintf(stderr, "ERROR: Stop here. Prevent deleting head. \n");
        exit(1);
    }
    if (((element -> prev) -> next) != element || ((element -> next) -> prev) != element)
    {
        return 1;
    }
    else
    {
        (element -> prev) ->next = element -> next;
        (element -> next) ->prev = element -> prev;
        free(element);
        return 0;
    }
    return 1;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
    if ((opt_yield & LOOKUP_YIELD) != 0)
    {
        sched_yield();
    }
    SortedListElement_t * temp = list -> next;
    while (temp -> key != NULL)
    {
        if (strcmp(temp->key, key) == 0)
        {
            return temp;
        }
        temp = temp-> next;
    }
    return NULL;
}

int SortedList_length(SortedList_t *list)
{
    if ((opt_yield & LOOKUP_YIELD) != 0)
    {
        sched_yield();
    }
    SortedListElement_t * temp = list-> next;
    int len = 0;
    while (temp-> key != NULL)
    {
        len = len + 1;
        temp = temp -> next;
    }
    return len;
}
