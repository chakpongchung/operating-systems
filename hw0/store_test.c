/*
 * A program that illustrates one approach to writing a test suite for
 * testing the task_store() function.  It is only to help you get 
 * started so you should expand and customize this program.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "task.h"

// define the struct for an entry in our storage
typedef struct task_entry {
    const char *key;
    task *task_ptr;
} task_entry;

/* 
 * Create an instance of the structures used to represent a task
 * They are made external so they are accessible to each test
 *
 */
  task my_task;
  VM my_vm;
  FS my_fs;
  paged my_paged;
  pinned my_pinned;

// organize as a suite of test functions each of which performs
// a specific set of tests
  void test1(void);
  void testdata(void);


void main (int argc, char *argv[])
{
  void *rc;

  printf("Test: Started\n");

  // initialize the pointers linking the individual structures
  // note that the individual test functions may overwrite
  // these values to create different test cases
  my_task.vm_ptr = &my_vm;
  my_task.fs_ptr = &my_fs;
  my_vm.paged_ptr = &my_paged;
  my_vm.pinned_ptr = &my_pinned;

  // be sure to initialize the task_store function
  rc = task_store(INIT, NULL, NULL);
  if (rc == NULL) {
    printf("Test: INIT failed\n");
    return;
  }
  else
    printf("Test: INIT successful\n");

  // invoke the first test function
  test1();

  // more test functions called here
  testdata();

  return;
}

void testdata(void)
{
  void *rc;

  // populate all the non-pointer fields
  // in this test, the initial pointer fields are used
  my_task.pid = (long)1234;
  my_fs.inode_start = (long)1000;
  my_fs.inode_end = (long)1000000;
  my_paged.paged_start = (void *)malloc(1024);
  my_paged.paged_end = (void *) malloc(16);
  my_pinned.pinned_start = (void *) malloc(2048);
  my_pinned.pinned_end = (void *) malloc(16);

  rc = task_store(STORE, "100", &my_task);
  task_entry *entry = (task_entry*) rc;
  printf("Task 1 pid: %ld\n", entry->task_ptr->pid);
  printf("Task 1 inode_start: %ld\n", entry->task_ptr->fs_ptr->inode_start);
  printf("Task 1 paged_start: %ld\n", (long)entry->task_ptr->vm_ptr->paged_ptr->paged_start);
  
  my_task.pid = (long)4321;
  my_fs.inode_start = (long)8888;
  my_fs.inode_end = (long)55555;
  my_paged.paged_start = (void *)malloc(1024);
  my_pinned.pinned_end = (void *)malloc(16);
  rc = task_store(STORE, "101", &my_task);
  task_entry *entry2 = (task_entry*) rc;
  printf("Task 1 pid: %ld\n", entry->task_ptr->pid);
  printf("Task 2 pid: %ld\n", entry2->task_ptr->pid);
  printf("Task 1 inode_start: %ld\n", entry->task_ptr->fs_ptr->inode_start);
  printf("Task 2 inode_start: %ld\n", entry2->task_ptr->fs_ptr->inode_start);
  printf("Task 1 paged_start: %ld\n", (long)entry->task_ptr->vm_ptr->paged_ptr->paged_start);
  printf("Task 2 paged_start: %ld\n", (long)entry2->task_ptr->vm_ptr->paged_ptr->paged_start);
  rc = task_store(LOCATE, "100 pid", NULL);
  if (rc == NULL) 
     printf("Test 1: LOCATE pid failed\n");
  else
     printf("Test 1: LOCATE pid %ld got %ld\n", (long)1234, *(long *)rc);
  rc = task_store(LOCATE, "101 pid", NULL);
  if (rc == NULL) 
     printf("Test 2: LOCATE pid failed\n");
  else
     printf("Test 2: LOCATE pid %ld got %ld\n", (long)4321, *(long *)rc);

  rc = task_store(LOCATE, "100 inode_end", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE inode_end failed\n");
  else
    printf("Test 1: LOCATE inode_end %ld got %ld\n", (long)1000000, *(long *)rc);
  rc = task_store(LOCATE, "101 inode_end", NULL);
  if (rc == NULL) 
    printf("Test 2: LOCATE inode_end failed\n");
  else
    printf("Test 2: LOCATE inode_end %ld got %ld\n", (long)55555, *(long *)rc);

  rc = task_store(LOCATE, "100 pinned_end", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE pinned_end failed\n");
  else
    printf("Test 1: LOCATE pinned_end %p got %p\n", entry->task_ptr->vm_ptr->pinned_ptr->pinned_end, *(void **)rc);
  rc = task_store(LOCATE, "101 pinned_end", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE pinned_end failed\n");
  else
    printf("Test 1: LOCATE pinned_end %p got %p\n", entry2->task_ptr->vm_ptr->pinned_ptr->pinned_end, *(void **)rc);


  my_task.fs_ptr = NULL;
  rc = task_store(STORE, "102", &my_task);
  task_entry *entry3 = (task_entry*) rc;
  rc = task_store(LOCATE, "102 inode_end", NULL);
  if (rc == NULL) 
    printf("Test 3: LOCATE inode_end success\n");
  else
    printf("Test 3: LOCATE inode_end failed\n");
  
  rc = task_store(STORE, "103", NULL);
  rc = task_store(LOCATE, "103 pid", NULL);
  if (rc == NULL) 
     printf("Test 4: LOCATE pid success\n");
  else
     printf("Test 4: LOCATE pid failed\n");
  rc = task_store(LOCATE, "103 pinned_start", NULL);
  if (rc == NULL) 
     printf("Test 4: LOCATE pinned_start success\n");
  else
     printf("Test 4: LOCATE pinned_start failed\n");
  
  rc = task_store(LOCATE, "103 foo", NULL);
  if (rc == NULL) 
     printf("Test 5: success\n");
  else
     printf("Test 5: failed\n");

  rc = task_store(LOCATE, "107 pid", NULL);
  if (rc == NULL) 
     printf("Test 6: success\n");
  else
     printf("Test 6: failed\n");
}

/*
 * This function performs the most basic test which is storing 
 * a task representation and then using LOCATE to return and
 * check the value of each field in the stored copy.
 */
void test1(void)
{
  void *rc;

  // populate all the non-pointer fields
  // in this test, the initial pointer fields are used
  my_task.pid = (long)1234;

  my_fs.inode_start = (long)1000;
  my_fs.inode_end = (long)1000000;

  my_paged.paged_start = (void *)malloc(1024);
  my_paged.paged_end = (void *) malloc(16);

  my_pinned.pinned_start = (void *) malloc(2048);
  my_pinned.pinned_end = (void *) malloc(16);

  // store the task representation
  rc = task_store(STORE, "100", &my_task);
  if (rc == NULL) {
    printf("Test 1: STORE failed\n");
    return;
  }
  else
    printf("Test  1: STORE successful\n");

  // retrieve all the fields and compare with the original values
  rc = task_store(LOCATE, "100 pid", NULL);
  if (rc == NULL) 
     printf("Test 1: LOCATE pid failed\n");
  else
     printf("Test 1: LOCATE pid %ld got %ld\n", (long)1234, *(long *)rc);

  rc = task_store(LOCATE, "100 paged_start", NULL);
  if (rc == NULL) 
     printf("Test 1: LOCATE paged_start failed\n");
  else
     printf("Test 1: LOCATE paged_start %p got %p\n", my_paged.paged_start, *(void **)rc);

  rc = task_store(LOCATE, "100 paged_end", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE paged_end failed\n");
  else
    printf("Test 1: LOCATE paged_end %p got %p\n", my_paged.paged_end, *(void **)rc);

  rc = task_store(LOCATE, "100 pinned_start", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE pinned_start failed\n");
  else
    printf("Test 1: LOCATE pinned_start %p got %p\n", my_pinned.pinned_start, *(void **)rc);

  rc = task_store(LOCATE, "100 pinned_end", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE pinned_end failed\n");
  else
    printf("Test 1: LOCATE pinned_end %p got %p\n", my_pinned.pinned_end, *(void **)rc);

  rc = task_store(LOCATE, "100 inode_start", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE inode_start failed\n");
  else
    printf("Test 1: LOCATE inode_start %ld got %ld\n", (long)1000, *(long *)rc);

  rc = task_store(LOCATE, "100 inode_end", NULL);
  if (rc == NULL) 
    printf("Test 1: LOCATE inode_end failed\n");
  else
    printf("Test 1: LOCATE inode_end %ld got %ld\n", (long)1000000, *(long *)rc);

  return;
}

