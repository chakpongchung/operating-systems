// define the debugfs path name directory and file
// full path name will be /sys/kernel/debug/getpid/call
char dir_name[] = "GPU_Lock";
char file_name[] = "call";

#define MAX_EVENTS 100 // characters in call request string
#define MAX_CALL 100 // characters in call request string
#define MAX_RESP 100 // characters in call request string
