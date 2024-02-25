#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
int main(int argc, char ** argv) {
	int p[2];
	pipe(p);
	if(fork() == 0) {
		char buf[5];
		/* this process is the child */
		read(p[0], buf, 5);
		close(p[0]);
		printf("%d: received %s\n", getpid(), buf);
		write(p[1], "pong", 5);
	} else {
		char buf[5];
		/* this process is the parent */
		write(p[1], "ping", 5);
		close(p[1]);

		read(p[0], buf, 5);
		printf("%d: received %s\n", getpid(), buf);
	}
	exit(0);
}
