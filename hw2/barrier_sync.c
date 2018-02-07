/* barrier_sync.c - A kernel service module for barrier-style synchronization
//
// David Dunn - Comp 790 HW2
//
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include "barrier_sync.h" /* used by both kernel module and user program */

int file_value;
struct dentry *dir, *file;  // used to set up debugfs file name

// set up array of wait queues
wait_queue_head_t queues[MAX_EVENTS];

/* The <integer-1> parameter is the character representation of the
integer value to be assigned as the event identifier for a new event. An attempt to create
an event using an already existing identifier is an error. This call is used to instantiate a
new event that includes a wait queue for processes blocked on it. On successful
completion, the module should return a character string containing only the <integer1>
value from the caller input string. If the operation fails for any reason, it should return
a string containing only the value -1. */
static int event_create(int queue){
  init_waitqueue_head(&queues[queue]);
  return sizeof(queues);
}

/*  The <integer-1> parameter is the character representation of the
integer event identifier the process is waiting to be signaled. The <integer-2>
character representation of an integer indicates exclusive (value 1) or nonexclusive
(value 0) treatment when the event is signaled. This call is used to
block a process by placing it on the event’s wait queue until the event is ‘signaled’ by
another process. Note that if successful, this call does not return in the user process (it
becomes blocked while executing inside your module) unless and until the event is
signaled or destroyed. This means that the write() call in the user process will not
complete until it is unblocked. It can then do the read() call to determine the result. On
successful completion, the module should return a character string containing only the
<integer-1> value from the caller input string. If the operation fails for any reason, it
should return a string containing only the value -1. */
static int event_wait(int queue, int exclusive){
  /*
  if (exclusive) {
    add_wait_queue_exclusive(q, &wait);
    prepare_to_wait_exclusive(&q, &wait, TASK_INTERRUPTIBLE);
  } else {
    add_wait_queue(q, &wait);
    prepare_to_wait(&q, &wait, TASK_INTERRUPTIBLE);
  }
  
  preempt_enable();
  // call schedule() here
  schedule();
  preempt_disable();
  finish_wait(&q, &wait);
  */
  return 2;
}

/*The <integer-1> parameter is the character representation of the
integer value of the event identifier to be signaled. This call is used to unblock one or
more processes (depending on the process exclusive/non-exclusive flag). On
successful completion, the module should return a character string containing only the
<integer-1> value from the caller input string. If the operation fails for any reason, it
should return a string containing only the value -1. */
static int event_signal(int queue){
  wake_up();
  return 3;
}

/*The <integer-1> parameter is the character representation of the
integer value of the event identifier to be destroyed. This call unblocks all blocked
processes independent of their exclusive/non-exclusive flag and makes the 
event and its wait queue unavailable to all processes (identifier is no longer valid until
another event_create is called with the same value). On successful completion, the
module should return a character string containing only the <integer-1> value from
the caller input string. If the operation fails for any reason, it should return a string
containing only the value -1. */
static int event_destroy(int queue){

  return 4;
}

static ssize_t barrier_sync_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  int rc;
  char callbuf[MAX_CALL];  // local (kernel) space to store call string
  char *token, *end;
  char oper[MAX_CALL];
  int param1, param2;

  if(count >= MAX_CALL)  // the user's write() call should not include a count that exceeds MAX_CALL
    return -EINVAL;  // return the invalid error code
  preempt_disable();  // protect static variables

  // get the call string
  rc = copy_from_user(callbuf, buf, count);
  callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

  // Tokenize the call string 
  /* Apparently I could have replaced this entire section with the 
     following sscanf(). 
     So much time wasted here!!! - live and learn
  rc = sscanf(callbuff, "%s %d %d", oper, &param1, &param2);
  if (rc != 2 && rc != 3){
    // do error stuff here
  }
  */
  end = callbuf;
  token = strsep(&end, " ");  // grab the first token (operator)
  if (token) { strcpy(oper, token); } // copy the token 
  else {
    // input call improperly formatted
    printk(KERN_DEBUG "barrier_sync: input call '%s %s' improperly formatted", token, callbuf);
    preempt_enable();  // clear the disable flag
    return -EINVAL;
  }
  token = strsep(&end, " ");  // grab the second token (integer-1)
  if (token) {
      rc = kstrtoint(token, 10, &param1);  //convert the parameter to int
      if (rc != 0) {         // it wasn't an integer
          printk(KERN_DEBUG "barrier_sync: second argument '%s' must be integer", token);
          preempt_enable();  // clear the disable flag
          return -EINVAL;
      }
  }
  else {
    // input call improperly formatted
    printk(KERN_DEBUG "barrier_sync: input call '%s %s' improperly formatted", token, callbuf);
    preempt_enable();  // clear the disable flag
    return -EINVAL;
  }
  token = strsep(&end, " ");  // grab the third token (integer-2)
  if (token) {  // This one is optional, so no eed to throw an error, unless there are more tokens
      rc = kstrtoint(token, 10, &param1);  //convert the parameter to int
      if (rc != 0) {         // it wasn't an integer
          printk(KERN_DEBUG "barrier_sync: second argument '%s' must be integer", token);
          preempt_enable();  // clear the disable flag
          return -EINVAL;
      }
    token = strsep(&end, " ");
    if (token) {
      // input call improperly formatted
      printk(KERN_DEBUG "barrier_sync: input call '%s %s' improperly formatted", token, callbuf);
      preempt_enable();  // clear the disable flag
      return -EINVAL;
    }
  } 

  // call the right function for the chosen operator
  if (strcmp(oper, "event_create") == 0) {
    rc = event_create(param1);
  } else if (strcmp(oper, "event_wait") == 0) {
    rc = event_wait(param1, param2);
  } else if (strcmp(oper, "event_signal") == 0) {
    rc = event_signal(param1);
  } else if (strcmp(oper, "event_destroy") == 0) {
    rc = event_destroy(param1);
  } else{ // invalid call
    rc = -1;
  }

  // store rc for the read() call later on
  

  // cleanup code at end
  printk(KERN_DEBUG "barrier_sync: call %s will return %s", callbuf, ret);
  preempt_enable();  // clear the disable flag
  *ppos = 0;  /* reset the offset to zero */
  return rc; // For Debugging
  return count;  /* write() calls return the number of bytes */
}

/* This function emulates the return from a system call by returning
 * the response to the user as a character string.  It is executed 
 * when the user program does a read() to the debugfs file used for 
 * emulating a system call.  The buf parameter points to a user space 
 * buffer, and count is a maximum size of the buffer space. 
 * 
 * The user space program is blocked at the read() call until this 
 * function returns.
 */
static ssize_t barrier_sync_return(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos) {
  int rc; 
  char ret[3];
  preempt_disable(); // protect static variables

  // convert rc to a string
  sprintf(ret, "%d", rc);

  preempt_enable(); // clear the disable flag
  *ppos = 0;  /* reset the offset to zero */
  return count;  /* read() calls return the number of bytes */
} 

// Defines the functions in this module that are executed
// for user read() and write() calls to the debugfs file
static const struct file_operations my_fops = {
        .read = barrier_sync_return,
        .write = barrier_sync_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */
static int __init barrier_sync_module_init(void) {
  /* create an in-memory directory to hold the file */
  dir = debugfs_create_dir(dir_name, NULL);
  if (dir == NULL) {
    printk(KERN_DEBUG "barrier_sync: error creating %s directory\n", dir_name);
     return -ENODEV;
  }
  /* create the in-memory file used for communication;
   * make the permission read+write by "world"
   */
  file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
  if (file == NULL) {
    printk(KERN_DEBUG "barrier_sync: error creating %s file\n", file_name);
     return -ENODEV;
  }
  printk(KERN_DEBUG "barrier_sync: created new debugfs directory and file\n");

  return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */
static void __exit barrier_sync_module_exit(void) {
  debugfs_remove(file);
  debugfs_remove(dir);
  //ToDo clean up any memory allocated for queues and responses
  //if (respbuf != NULL)
  //   kfree(respbuf);
}

/* Declarations required in building a module */
module_init(barrier_sync_module_init);
module_exit(barrier_sync_module_exit);
MODULE_LICENSE("GPL");
