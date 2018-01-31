#define MAX_CALL 100 // characters in call request string
#define MAX_LINE 100 // characters in a single pid response string
#define MAX_ENTRY 200 // total characters in buffer
#define MAX_SIBLINGS 5  // total number of entrys to return (including caller)
// define the debugfs path name directory and file
// full path name will be /sys/kernel/debug/getpid/call
char dir_name[] = "getpid";
char file_name[] = "call";


