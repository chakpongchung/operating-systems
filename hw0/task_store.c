#include "task.h"
#include <stddef.h>

#define MAXSIZE 50   // maximum number of tasks allowed
int num_tasks = 0;   // num of tasks used

// define the struct for an entry in our storage
typedef struct task_entry {
    const char *key;
    task *ptr;
} task_entry;

task_entry *data;   // array where data will be stored

// Will initialize the data storage
void *init(char *parm, task *ptr){
    data = (task_entry *) malloc(MAXSIZE*sizeof(task_entry));
    return data;
}

// Will store a deep copy of a task in our data structure
void *store(char *parm, task *ptr){
    if (!data) return NULL;                     // check if init is called
    if (num_tasks == MAXSIZE - 1) return NULL;  // check maximum size exceeded

    return NULL;
}

// Will locate a task in our data structure
void *locate(char *parm, task *ptr){
    if (!data) return NULL;                     // check if init is called

    return NULL;
}


// Will perform one of three operations on a task structure:
//      1 initialize data structures associated with task_store
//      2 store a copy of a task structure
//      3 return a pointer to an element of a previously stored copy of a task
void *task_store(enum operation op, char *parm, task *ptr){
    switch(op){
        case INIT: return init(parm, ptr);
        case STORE: return store(parm, ptr);
        case LOCATE: return locate(parm, ptr);
        default: break;
    }
    return NULL;
}
