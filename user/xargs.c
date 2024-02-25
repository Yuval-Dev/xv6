#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int
main(int argc, char *argv[]){
    char ** new_argv = malloc((argc + 1) * sizeof(char *));
    for(int i = 1; i < argc; i++){
        new_argv[i - 1] = argv[i];
    }
	char arg_buf[8192];
    new_argv[argc - 1] = arg_buf;
    new_argv[argc] = 0;

    char buf;
    int i = 0;
    while(read(0, &buf, 1)) {
        if(buf=='\n' || buf == '\0'){
            new_argv[argc - 1][i++] = '\0';
            if(fork() == 0) {
                exec(argv[1], new_argv);
				exit(-1);
            }
			i = 0;
			wait(0);
        } else {
            new_argv[argc - 1][i++] = buf;
        }
    }
    exit(0);
}
