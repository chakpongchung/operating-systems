/* mutex.c - A kernel service module for providing locks
//
// David Dunn - Comp 790 HW3
//
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "GPU_Locks_kernel.h" /* used by both kernel module and user program */

int file_value;
struct dentry *dir, *file;  // used to set up debugfs file name

DEFINE_MUTEX(ce_mutex);
pid_t ce_lock_pid = 0; // the pid that holds the lock
DEFINE_MUTEX(ee_mutex);
pid_t ee_lock_pid = 0; // the pid that holds the lock

/*  */
static int ce_lock(){
  printk(KERN_DEBUG "mutex: entering ce_lock  \n");  // goes into /var/log/kern.log
  mutex_lock(&ce_mutex);
  ce_lock_pid = task_pid_nr(current);
  return 0;
}

/*  */
static int ce_unlock(){
  pid_t my_pid;
  printk(KERN_DEBUG "mutex: entering ce_unlock  \n");  // goes into /var/log/kern.log
  my_pid = task_pid_nr(current);
  if (my_pid != ce_lock_pid) return -1;  // if this is some other process that doesn't hold the lock, return
  mutex_unlock(&ce_mutex);
  ce_lock_pid = 0;
  return 0;
}

/*  */
static int ee_lock(){
  printk(KERN_DEBUG "mutex: entering ee_lock  \n");  // goes into /var/log/kern.log
  mutex_lock(&ee_mutex);
  ee_lock_pid = task_pid_nr(current);
  return 0;
}

/*  */
static int ee_unlock(){
  printk(KERN_DEBUG "mutex: entering ee_unlock  \n");  // goes into /var/log/kern.log
  my_pid = task_pid_nr(current);
  if (my_pid != ee_lock_pid) return -1;  // if this is some other process that doesn't hold the lock, return
  mutex_unlock(&ee_mutex);
  ee_lock_pid = 0;
  return 0;
}


/* This function emulates the handling of a system call by accessing the call string from
the user program, executing the requested function and storing a response. This function is 
executed when a user program does a write() to the debugfs file used for emulating a system 
call.  The buf parameter points to a user space buffer, and count is a maximum size of the 
buffer content. The user space program is blocked at the write() call until this function 
returns. */
static ssize_t mutex_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  int rc;
  char callbuf[MAX_CALL];  // local (kernel) space to store call string
  char oper[MAX_CALL];
  
  printk(KERN_DEBUG "mutex: entering call  \n");  // goes into /var/log/kern.log
  
  if(count >= MAX_CALL)  // the user's write() call should not include a count that exceeds MAX_CALL
    return -EINVAL;  // return the invalid error code
  preempt_disable();  // protect static variables

  // get the call string
  rc = copy_from_user(callbuf, buf, count);
  callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

  printk(KERN_DEBUG "mutex: read the string: %s   \n",callbuf);  // goes into /var/log/kern.log
  // Tokenize the call string 
  rc = sscanf(callbuff, "%s", oper);
  if (rc != 1){
    preempt_enable(); 
    return -ENOSPC;
  }
  
  // prepare storage for returning value
  my_retval = kmalloc(sizeof(retval), GFP_ATOMIC);
  if (my_retval == NULL) {  // test if allocation failed
    preempt_enable(); 
    return -ENOSPC;
  }
  //printk(KERN_DEBUG "mutex: allocated storage     my_retval = 0x%08x   ",my_retval);  // goes into /var/log/kern.log

  // call the right function for the chosen operator
  if (strcmp(oper, "CE_Lock") == 0) {
    rc = ce_lock();
  } else if (strcmp(oper, "CE_UnLock") == 0) {
    rc = ce_unlock();
  } else if (strcmp(oper, "EE_Lock") == 0) {
    rc = ee_lock();
  } else if (strcmp(oper, "EE_UnLock") == 0) {
    rc = ee_unlock();
  } else{ // invalid call
    rc = -1;
  }
  printk(KERN_DEBUG "mutex: returned from function     rc = %d   \n",rc);  // goes into /var/log/kern.log

  // cleanup code at end
  printk(KERN_DEBUG "mutex: call %s will return %d\n", callbuf, rc);  // goes into /var/log/kern.log
  preempt_enable();  // clear the disable flag
  *ppos = 0;  /* reset the offset to zero */
  return rc; // For this module only return the code
  //return count;  /* write() calls return the number of bytes */
}

/* This function emulates the return from a system call by returning the response to 
the user as a character string.  It is executed  when the user program does a read() 
to the debugfs file used for emulating a system call.  The buf parameter points to a 
user space buffer, and count is a maximum size of the buffer space. The user space 
program is blocked at the read() call until this function returns. */
static ssize_t mutex_return(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos) {
  int rc = 0; 
  //printk(KERN_DEBUG "mutex: entering return  ret_list = 0x%08x", ret_list);  // goes into /var/log/kern.log
  return rc;  // For this module only return the code
} 

// Defines the functions in this module that are executed
// for user read() and write() calls to the debugfs file
static const struct file_operations my_fops = {
        .read = mutex_return,
        .write = mutex_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */
static int __init mutex_module_init(void) {
  /* create an in-memory directory to hold the file */
  dir = debugfs_create_dir(dir_name, NULL);
  if (dir == NULL) {
    printk(KERN_DEBUG "mutex: error creating %s directory\n", dir_name);
     return -ENODEV;
  }
  /* create the in-memory file used for communication;
   * make the permission read+write by "world"
   */
  file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
  if (file == NULL) {
    printk(KERN_DEBUG "mutex: error creating %s file\n", file_name);
     return -ENODEV;
  }
  printk(KERN_DEBUG "mutex: created new debugfs directory and file\n");

  return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */
static void __exit mutex_module_exit(void) {
  debugfs_remove(file);
  debugfs_remove(dir);
}

/* Declarations required in building a module */
module_init(mutex_module_init);
module_exit(mutex_module_exit);
MODULE_LICENSE("GPL");
