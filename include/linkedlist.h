// DO NOT MODIFY THIS FILE
// Any additions should be placed in helpers2.h

#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    void* data;          // pointer to the data to be stored
    struct node* next;   // pointer to the next node in the linked list
} node_t;

typedef struct list {
    node_t* head;        // pointer to the node_t at the head of the list
    int length;          // number of items in the list
    int (*comparator)(void*, void*);        // function pointer for in order insertion
    void (*printer)(void*, void*, char *);  // function pointer for printing the data stored
    void (*deleter)(void*);                 // function pointer for deleting any dynamically 
                                            // allocated items within the data stored
} list_t;

// Functions already implemented
list_t* CreateList(int (*compare)(void*, void*), void (*print)(void*,void*,char*),
                   void (*delete)(void*));
void InsertAtHead(list_t* list, void* val_ref);
void InsertAtTail(list_t* list, void* val_ref);
void InsertInOrder(list_t* list, void* val_ref);
void PrintLinkedList(list_t* list, FILE* fp, char* sep);

#endif
