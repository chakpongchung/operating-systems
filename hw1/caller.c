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

#include "getpinfo.h" /* used by both kernel module and user program */

void do_syscall(char *call_string);  // does the call emulation

// variables shared between main() and the do_syscall() function
int fp;
char the_file[256] = "/sys/kernel/debug/";
char call_buf[MAX_CALL];  /* no call string can be longer */
char resp_buf[MAX_RESP];  /* no response strig can be longer */

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

  // use the system call to get the pid
  //my_pid = getpid();
  //fprintf(stdout, "System call getpinfo() returns %d\n", my_pid);

  // use the kernel module to get the pid
  do_syscall("getpinfo");

  fprintf(stdout, "%s", resp_buf);

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

  // TODO - man 2 write and man 2 read to get the return codes and define those in the module
  rc = write(fp, call_buf, strlen(call_buf) + 1);
  if (rc == -1) {
     fprintf (stderr, "error writing %s\n", the_file);
     fflush(stderr);
     exit (-1);
  }

  rc = read(fp, resp_buf, sizeof(resp_buf));
  if (rc == -1) {
     fprintf (stderr, "error reading %s\n", the_file);
     fflush(stderr);
     exit (-1);
  }
}

