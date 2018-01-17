#include "task.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAXSIZE 50   // maximum number of tasks allowed
int num_tasks = 0;   // num of tasks used

// define the struct for an entry in our storage
typedef struct task_entry {
    const char *key;
    task *my_task;
} task_entry;

task_entry *data;   // array where data will be stored

// Will initialize the data storage
void *init(){
    data = (task_entry *) malloc(MAXSIZE*sizeof(task_entry));
    return data;
}

// Will store a deep copy of a task in our data structure
void *store(char *parm, task *ptr){
    if (!data) return NULL;                     // check if init is called
    if (num_tasks == MAXSIZE - 1) return NULL;  // check maximum size exceeded

    // copy task
    task *my_task;
    my_task = (task *) malloc(sizeof(task));
    *my_task = *ptr;
    // copy FS
    if (my_task->fs_ptr){
        FS *my_fs;
        my_fs = (FS *) malloc(sizeof(FS));
        *my_fs = *(my_task->fs_ptr);
        my_task->fs_ptr = my_fs;
    }
    // copy VM
    if (my_task->vm_ptr){
        VM *my_vm;
        my_vm = (VM *) malloc(sizeof(VM));
        *my_vm = *(my_task->vm_ptr);
        my_task->vm_ptr = my_vm;
        // copy paged
        if (my_vm->paged_ptr){
            paged *my_paged;
            my_paged = (paged *) malloc(sizeof(paged));
            *my_paged = *(my_vm->paged_ptr);
            my_vm->paged_ptr = my_paged;
        }
        // copy pinned
        if (my_vm->pinned_ptr){
            pinned *my_pinned;
            my_pinned = (pinned *) malloc(sizeof(pinned));
            *my_pinned = *(my_vm->pinned_ptr);
            my_vm->pinned_ptr = my_pinned;
        }
    }
    // add our copied task into our array
    task_entry new_entry = {.key = parm, .my_task = my_task};
    *(data+num_tasks) = new_entry;
    num_tasks += 1;
    return data+num_tasks-1;
}

// Will locate a task in our data structure
void *locate(char *parm){
    if (!data) return NULL;                     // check if init is called
    char *key;
    char *field;
    key = strtok(parm, " ");
    field = strtok(NULL, " ");
    printf("Key: %s \t Field: %s\n",key,field)
    return NULL;
}


// Will perform one of three operations on a task structure:
//      1 initialize data structures associated with task_store
//      2 store a copy of a task structure
//      3 return a pointer to an element of a previously stored copy of a task
void *task_store(enum operation op, char *parm, task *ptr){
    switch(op){
        case INIT: return init();
        case STORE: return store(parm, ptr);
        case LOCATE: return locate(parm);
        default: break;
    }
    return NULL;
}
