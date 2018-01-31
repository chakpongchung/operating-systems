/*
 * A kernel loadable module for getting the process info and returning
 * it as a new-line delimited string
 */ 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/rwsem.h>

#include "getpinfo.h" /* used by both kernel module and user program 
                     * to define shared parameters including the
                     * debugfs directory and file used for emulating
                     * a system call
                     */

/* The following two variables are global state shared between
 * the "call" and "return" functions.  They need to be protected
 * from re-entry caused by kernel preemption.
 */
/* The call_task variable is used to ensure that the result is
 * returned only to the process that made the call.  Only one
 * result can be pending for return at a time (any call entry 
 * while the variable is non-NULL is rejected).
 */

struct task_struct *call_task = NULL;
char *respbuf;  // points to memory allocated to return the result

int file_value;
struct dentry *dir, *file;  // used to set up debugfs file name

static int gen_pinfo_string(char *buf, struct task_struct *tsk);

/* This function emulates the handling of a system call by
 * accessing the call string from the user program, executing
 * the requested function and preparing a response.
 *
 * This function is executed when a user program does a write()
 * to the debugfs file used for emulating a system call.  The
 * buf parameter points to a user space buffer, and count is a
 * maximum size of the buffer content.
 *
 * The user space program is blocked at the write() call until
 * this function returns.
 */

static ssize_t getpinfo_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  int rc;
  char callbuf[MAX_CALL];  // local (kernel) space to store call string
  
  // the user's write() call should not include a count that exceeds MAX_CALL
  if(count >= MAX_CALL)
    return -EINVAL;  // return the invalid error code
  
  // Prevent preemption.  
  preempt_disable();  
  if (call_task != NULL) { // a different process is expecting a return
     preempt_enable();  // must be enabled before return
     return -EAGAIN;
  }

  // allocate some kernel memory for the response
  respbuf = kmalloc(MAX_ENTRY * MAX_SIBLINGS, GFP_ATOMIC);
  if (respbuf == NULL) {  // test if allocation failed
     preempt_enable(); 
     return -ENOSPC;
  }
  strcpy(respbuf,""); /* initialize buffer with null string */
  
  call_task = current;  // this returns a pointer to the structure of the calling task
  rc = copy_from_user(callbuf, buf, count);
  callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

  if (strcmp(callbuf, "getpinfo") != 0) { // only valid call is "getpinfo"
      strcpy(respbuf, "Failed: invalid operation\n");
      printk(KERN_DEBUG "getpinfo: call %s will return %s\n", callbuf, respbuf);  // goes into /var/log/kern.log
      preempt_enable();
      return count;  /* write() calls return the number of bytes written */
  }

  // generate the pinfo string for the current process
  rc = gen_pinfo_string(respbuf, call_task);

  // cleanup code at end
  printk(KERN_DEBUG "getpinfo: call %s will return %s", callbuf, respbuf);
  preempt_enable();
  *ppos = 0;  /* reset the offset to zero */
  return count;  /* write() calls return the number of bytes */
}

  /* This function parses the task_struct and formats a string with the info
   * The string will include:
   * Current PID 5234
   *   command caller      (comm)
   *   parent PID 1954     (real_parent)
   *   state 0             (state)
   *   flags 0x00406000    (flags) in hex
   *   priority 120        (normal_prio)
   *   VM areas 15         (map_count)
   *   VM shared 464       (shared_vm)
   *   VM exec 457         (exec_vm)
   *   VM stack 34         (stack_vm)
   *   VM total 507        (total_vm)
   */
static int gen_pinfo_string(char *buf, struct task_struct *tsk)
{
  char resp_line[MAX_LINE]; // local (kernel) space for a response (before cat into buf)
  pid_t cur_pid = 0;
  pid_t par_pid = 0;
  char comm[sizeof(tsk->comm)+1];

  cur_pid = task_pid_nr(tsk); //Use kernel functions for access to pid for a process 
  sprintf(buf, "Current PID %d\n", cur_pid); // start forming a response in the buffer

  get_task_comm(comm, tsk);  // use kernel function for access to command name
  sprintf(resp_line, "  command %s\n", comm);
  strcat(buf, resp_line);
  
  par_pid = task_pid_nr(tsk->real_parent);
  sprintf(resp_line, "  parent PID %d\n", par_pid);
  strcat(respbuf, resp_line);
  
  sprintf(resp_line, "  state %ld\n", tsk->state);
  strcat(respbuf, resp_line);
  
  sprintf(resp_line, "  flags 0x%08x\n", tsk->flags);
  strcat(respbuf, resp_line);
  
  sprintf(resp_line, "  priority %d\n", tsk->normal_prio);
  strcat(respbuf, resp_line);

  // Virtual Memory critical section
  down_read(tsk->mm->mmap_sem);
  sprintf(resp_line, "  VM areas %d\n", tsk->mm->map_count);
  strcat(respbuf, resp_line);
  
  sprintf(resp_line, "  VM shared %ld\n", tsk->mm->shared_vm);
  strcat(respbuf, resp_line);

  sprintf(resp_line, "  VM exec %ld\n", tsk->mm->exec_vm);
  strcat(respbuf, resp_line);

  sprintf(resp_line, "  VM stack %ld\n", tsk->mm->stack_vm);
  strcat(respbuf, resp_line);

  sprintf(resp_line, "  VM total %ld\n", tsk->mm->total_vm);
  strcat(respbuf, resp_line);
  
  up_read(tsk->mm->mmap_sem);
  // End Virtual memory critical section

  return 0;
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

static ssize_t getpinfo_return(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos)
{
  int rc; 

  preempt_disable(); // protect static variables

  if (current != call_task) { // return response only to the process making
                              // the getpinfo request
     preempt_enable();
     return 0;  // a return of zero on a read indicates no data returned
  }

  rc = strlen(respbuf) + 1; /* length includes string termination */

  /* return at most the user specified length with a string 
   * termination as the last byte.  Use the kernel function to copy
   * from kernel space to user space.
   */

  /* Use the kernel function to copy from kernel space to user space.
   */
  if (count < rc) { // user's buffer is smaller than response string
    respbuf[count - 1] = '\0'; // truncate response string
    rc = copy_to_user(userbuf, respbuf, count); // count is returned in rc
  }
  else 
    rc = copy_to_user(userbuf, respbuf, rc); // rc is unchanged

  kfree(respbuf); // free allocated kernel space

  respbuf = NULL;
  call_task = NULL; // response returned so another request can be done

  preempt_enable(); // clear the disable flag

  *ppos = 0;  /* reset the offset to zero */
  return rc;  /* read() calls return the number of bytes */
} 

// Defines the functions in this module that are executed
// for user read() and write() calls to the debugfs file
static const struct file_operations my_fops = {
        .read = getpinfo_return,
        .write = getpinfo_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */

static int __init getpinfo_module_init(void)
{

  /* create an in-memory directory to hold the file */

  dir = debugfs_create_dir(dir_name, NULL);
  if (dir == NULL) {
    printk(KERN_DEBUG "getpinfo: error creating %s directory\n", dir_name);
     return -ENODEV;
  }

  /* create the in-memory file used for communication;
   * make the permission read+write by "world"
   */

  file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
  if (file == NULL) {
    printk(KERN_DEBUG "getpinfo: error creating %s file\n", file_name);
     return -ENODEV;
  }

  printk(KERN_DEBUG "getpinfo: created new debugfs directory and file\n");

  return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */

static void __exit getpinfo_module_exit(void)
{
  debugfs_remove(file);
  debugfs_remove(dir);
  if (respbuf != NULL)
     kfree(respbuf);
}

/* Declarations required in building a module */
module_init(getpinfo_module_init);
module_exit(getpinfo_module_exit);
MODULE_LICENSE("GPL");
