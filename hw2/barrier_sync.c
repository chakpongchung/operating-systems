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
wait_queue_head_t *queues[MAX_EVENTS] = {NULL};

// structure for listhead of return values
typedef struct {
  pid_t tsk;
  int rc;
  struct list_head list;
} retval;
static LIST_HEAD(ret_list);

/* The <integer-1> parameter is the character representation of the
integer value to be assigned as the event identifier for a new event. An attempt to create
an event using an already existing identifier is an error. This call is used to instantiate a
new event that includes a wait queue for processes blocked on it. On successful
completion, the module should return a character string containing only the <integer1>
value from the caller input string. If the operation fails for any reason, it should return
a string containing only the value -1. */
static int event_create(int queue){
  if (queues[queue]) return -5;  // check to see if already init
  // allocate some kernel memory for the response
  queues[queue] = (wait_queue_head_t*) kmalloc(sizeof(wait_queue_head_t), GFP_ATOMIC);
  if (queues[queue] == NULL) {  // test if allocation failed
     return -ENOSPC;
  }
  init_waitqueue_head(queues[queue]);
  return queue;
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
  if (queues[queue] == NULL) return -6;  // check to see if already init
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
  if (queues[queue] == NULL) return -6;  // check to see if already init
  //wake_up(&(queues[queue]));
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
  if (queues[queue] == NULL) return -6;  // check to see if already init
  //wake_up_all(&(queues[queue]));
  kfree(queues[queue]);
  queues[queue] = NULL;
  return 4;
}

/* This function emulates the handling of a system call by accessing the call string from
the user program, executing the requested function and storing a response. This function is 
executed when a user program does a write() to the debugfs file used for emulating a system 
call.  The buf parameter points to a user space buffer, and count is a maximum size of the 
buffer content. The user space program is blocked at the write() call until this function 
returns. */
static ssize_t barrier_sync_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  int rc;
  char callbuf[MAX_CALL];  // local (kernel) space to store call string
  char *token, *end;
  char oper[MAX_CALL];
  int param1, param2;
  retval *my_retval;

  printk(KERN_DEBUG "barrier_sync: entering call  ");  // goes into /var/log/kern.log
  
  if(count >= MAX_CALL)  // the user's write() call should not include a count that exceeds MAX_CALL
    return -EINVAL;  // return the invalid error code
  preempt_disable();  // protect static variables

  // get the call string
  rc = copy_from_user(callbuf, buf, count);
  callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

  printk(KERN_DEBUG "barrier_sync: read the string: %s   ",callbuf);  // goes into /var/log/kern.log
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
      rc = kstrtoint(token, 10, &param2);  //convert the parameter to int
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
  printk(KERN_DEBUG "barrier_sync: tokenized the string: %s   ",oper);  // goes into /var/log/kern.log
  
  // prepare storage for returning value
  my_retval = kmalloc(sizeof(retval), GFP_ATOMIC);
  if (my_retval == NULL) {  // test if allocation failed
     preempt_enable(); 
     return -ENOSPC;
  }
  printk(KERN_DEBUG "barrier_sync: allocated storage     my_retval = 0x%08x   ",my_retval);  // goes into /var/log/kern.log

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
    rc = -100;
  }
  printk(KERN_DEBUG "barrier_sync: returned from function     rc = %d   ",rc);  // goes into /var/log/kern.log

  // store rc for the read() call later on
  my_retval->tsk = task_pid_nr(current);
  my_retval->rc = rc;
  INIT_LIST_HEAD(&my_retval->list);
  list_add(&my_retval->list, &ret_list);

  printk(KERN_DEBUG "barrier_sync: ending call  ret_list = 0x%08x", ret_list);  // goes into /var/log/kern.log
  printk(KERN_DEBUG "barrier_sync: ending call  my_retval->list = 0x%08x", my_retval->list);  // goes into /var/log/kern.log
  printk(KERN_DEBUG "barrier_sync: ending call  my_retval = 0x%08x", my_retval);  // goes into /var/log/kern.log
  
  // cleanup code at end
  printk(KERN_DEBUG "barrier_sync: call %s will return %d", callbuf, rc);  // goes into /var/log/kern.log
  preempt_enable();  // clear the disable flag
  *ppos = 0;  /* reset the offset to zero */
  return rc; // For Debugging
  return count;  /* write() calls return the number of bytes */
}

/* This function emulates the return from a system call by returning the response to 
the user as a character string.  It is executed  when the user program does a read() 
to the debugfs file used for emulating a system call.  The buf parameter points to a 
user space buffer, and count is a maximum size of the buffer space. The user space 
program is blocked at the read() call until this function returns. */
static ssize_t barrier_sync_return(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos) {
  int rc = -1; 
  char respbuf[3];
  retval *my_retval, *next;
  pid_t cur_pid;
  
  printk(KERN_DEBUG "barrier_sync: entering return  ret_list = 0x%08x", ret_list);  // goes into /var/log/kern.log
  
  preempt_disable(); // protect static variables
  cur_pid = task_pid_nr(current);
  printk(KERN_DEBUG "barrier_sync: starting loop  my_retval = 0x%08x", my_retval);  // goes into /var/log/kern.log
  /*
  list_for_each_entry_safe(my_retval, next, &ret_list, list){
    printk(KERN_DEBUG "barrier_sync: inside loop  my_retval = 0x%08x", my_retval);  // goes into /var/log/kern.log
    printk(KERN_DEBUG "barrier_sync: inside loop       next = 0x%08x", next);  // goes into /var/log/kern.log
    if(my_retval->tsk == cur_pid){
      printk(KERN_DEBUG "barrier_sync: inside if  my_retval->rc = %d", my_retval->rc);  // goes into /var/log/kern.log
      rc = my_retval->rc;
      list_del(&my_retval->list);
      printk(KERN_DEBUG "barrier_sync: deleted    my_retval = 0x%08x", my_retval);  // goes into /var/log/kern.log
      kfree(my_retval);
      printk(KERN_DEBUG "barrier_sync: freed      my_retval = 0x%08x", my_retval);  // goes into /var/log/kern.log
      break;
    }
  }
  // convert rc to a string
  sprintf(respbuf, "%d", rc);
  printk(KERN_DEBUG "barrier_sync: converted to string     respbuf = %s", respbuf);  // goes into /var/log/kern.log
      
  // Use the kernel function to copy from kernel space to user space.
  if (count < strlen(respbuf)) { // user's buffer is smaller than response string
    respbuf[count - 1] = '\0'; // truncate response string
    rc = copy_to_user(userbuf, respbuf, count); // count is returned in rc
  }
  else 
    rc = copy_to_user(userbuf, respbuf, rc); // rc is unchanged
  printk(KERN_DEBUG "barrier_sync: finished copying to user");  // goes into /var/log/kern.log
  */
  preempt_enable(); // clear the disable flag
  *ppos = 0;  /* reset the offset to zero */
  printk(KERN_DEBUG "barrier_sync: about to return     count = %d", count);  // goes into /var/log/kern.log
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
  int i;
  debugfs_remove(file);
  debugfs_remove(dir);
  // clean up any memory allocated for queues ToDo and responses
  for (i=0;i<MAX_EVENTS;i++){
    kfree(queues[i]);
    queues[i] = NULL;
  }
}

/* Declarations required in building a module */
module_init(barrier_sync_module_init);
module_exit(barrier_sync_module_exit);
MODULE_LICENSE("GPL");
