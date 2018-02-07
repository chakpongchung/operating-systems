/*
 * A simple user space program to illustrate calling an
 * emulated "system call" in programming assignments in
 * COMP 790-042.  It opens the debugfs file used for calling
 * the getpinfo kernel module, requests the pid of the calling
 * process, and outputs the result.  It also outputs the 
 * result from the regular Linux getpinfo() system call so the
 * two results can be compared.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "barrier_sync.h" /* used by both kernel module and user program */

void do_syscall(char *call_string);  // does the call emulation

// variables shared between main() and the do_syscall() function
int fp;
char the_file[256] = "/sys/kernel/debug/";
char call_buf[MAX_CALL];  /* no call string can be longer */
char resp_buf[MAX_RESP];  /* no response string can be longer */

void main (int argc, char* argv[])
{
  int i;
  int rc = 0;
  pid_t my_pid;  

  /* Build the complete file path name and open the file */

  strcat(the_file, dir_name);
  strcat(the_file, "/");
  strcat(the_file, file_name);

  if ((fp = open (the_file, O_RDWR)) == -1) {
      fprintf (stderr, "error opening %s\n", the_file);
      exit (-1);
  }

  // Call the module with the call string
  do_syscall("getpinfo 0 0");
  do_syscall("event_create");
  do_syscall("event_create a");
  do_syscall("event_create 0");
  do_syscall("event_create 0 a");
  do_syscall("event_create 0 1");
  do_syscall("event_create 0 1 a");
  do_syscall("event_wait 0 1");
  do_syscall("event_signal 1");
  do_syscall("event_destroy 0");
  do_syscall("event_wait 0 1");

  //fprintf(stdout, "%s", resp_buf);

  close (fp);
} /* end main() */

/* 
 * A function to actually emulate making a system call by
 * writing the request to the debugfs file and then reading
 * the response.  It encapsulates the semantics of a regular
 * system call in that the calling process is blocked until
 * both the request (write) and response (read) have been
 * completed by the kernel module.
 *  
 * The input string should be properly formatted for the
 * call string expected by the kernel module using the
 * specified debugfs path (this function does no error
 * checking of input).
 */ 

void do_syscall(char *call_string)
{
  int rc;

  strcpy(call_buf, call_string);

  rc = write(fp, call_buf, strlen(call_buf) + 1);
  fprintf(stdout, "Call: '%s'     \t- rc: %d \t- errno: %d\n", call_string, rc, errno);
  fflush(stdout);
  /*if (rc == -1) {
     fprintf (stderr, "error writing %s\n", the_file);
     fflush(stderr);
     exit (-1);
  }*/

  /*rc = read(fp, resp_buf, sizeof(resp_buf));
  fprintf(stdout, "Response: '%s' - rc: %d\n", call_string, rc);
  fflush(stdout);
  */
  /*if (rc == -1) {
     fprintf (stderr, "error reading %s\n", the_file);
     fflush(stderr);
     //exit (-1);
  }
  */
}
