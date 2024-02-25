#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/syscall.h"
int main(int argc, char ** argv) {
	if(argc < 3) {
		printf("Usage: trace <mask> <command...>\n");
		exit(1);
	}
	int mask = atoi(argv[1]);
	trace(mask);
	char ** new_argv = malloc(sizeof(char *) * (argc - 1));
	for(int i = 2; i < argc; i++) {
		new_argv[i - 2] = argv[i];
	}
	new_argv[argc - 2] = 0;
	exec(argv[2], new_argv);
	int other_pid;
	wait(&other_pid);
	exit(0);
}
