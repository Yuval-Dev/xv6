#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
int main(int argc, char ** argv) {
	int nums[40];
	for(int i = 0; i < 40; i++) {
		nums[i] = i + 2;
	}
	int cnt_nums = 40;
	while(cnt_nums != 0) {
		int tmp_nums[40];
		int p[2];
		pipe(p);
		if(fork() == 0) {
			int cnt_read = read(p[0], tmp_nums, 200);
			close(p[0]);
			int tmp_cnt_nums = cnt_read / sizeof(int);
			int prime = tmp_nums[0];
			fprintf(2, "prime %d\n", prime); 
			cnt_nums = 0;
			for(int i = 1; i < tmp_cnt_nums; i++) {
				if(tmp_nums[i] % prime == 0) continue;
				nums[cnt_nums] = tmp_nums[i];
				cnt_nums++;
			}
		} else {
			write(p[1], nums, cnt_nums * sizeof(int));
			close(p[1]);
			break;
		}
	}
	int status;
	wait(&status);
	if(status != 0) {
		exit(-1);
	} else {
		exit(0);
	}
}
