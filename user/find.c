#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

int is_match(char * full_path, char * target) {
	char * it = full_path;
	while(*it != '\0') {
		it++;
	}
	while(it > full_path && *it != '/') {
		it--;
	}
	it++;
	return strcmp(it, target);
}

void find(char * path, char * target) {
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;
	if(is_match(path, target) == 0) {
		fprintf(1, "%s\n", path);
	}
	if((fd = open(path, 0)) < 0){
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "find: cannot stat %s\n", path);
		close(fd);
		return;
	}
	if(st.type == T_DIR) {
		if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
			close(fd);
			return;
		}
		strcpy(buf, path);
		p = buf+strlen(buf);
		*p++ = '/';
		while(read(fd, &de, sizeof(de)) == sizeof(de)){
			if(de.inum == 0)
				continue;
			memmove(p, de.name, DIRSIZ);
			p[DIRSIZ] = 0;
			int buf_len = strlen(buf);
			if(buf[buf_len - 1] != '.') {
				find(buf, target);
			}
		}
	}
	close(fd);
}

int main(int argc, char ** argv) {
	if(argc < 3) {
		fprintf(2, "Usage: find <dir> <text>\n");
		exit(1);
	}
	find(argv[1], argv[2]);
	exit(0);
}
