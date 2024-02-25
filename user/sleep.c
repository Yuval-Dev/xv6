#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "user/yuval_lib.h"

int main(int argc, char ** argv) {
	if(argc < 2) {
		fprintf(2, "Usage: sleep <delay>\n");
		exit(1);
	}
	char * delay_arg = argv[1];
	long int delay_time = strtol(delay_arg, &delay_arg, 10);
	if(delay_arg == argv[1]) {
		fprintf(2, "Expected int, %s is not an int :(\n", argv[1]);
		exit(2);
	}
	sleep(delay_time);
	exit(0);
}
