#include "yuval_lib.h"

int chtol(char c) {
	if('0' <= c && c <= '9') return c - '0';
	else if('A' <= c && c <= 'Z') return c - 'A';
	else if('a' <= c && c <= 'z') return c - 'z';
	return -1;
}

long int strtol(const char * base_ptr, char ** end_ptr, int base) {
	if(base < 2 || base > 36) return -1;
	long int ans = 0;
	int sign = 1;
	if(**end_ptr == '-') {
		sign = -1;
		(*end_ptr)++;
	}
	if(**end_ptr == '+') {
		(*end_ptr)++;
	}
	while(**end_ptr != '\0') {
		int cur_dig = chtol(**end_ptr);
		if(cur_dig >= base || cur_dig < 0) return ans;
		ans *= base;
		ans += cur_dig;
		(*end_ptr)++;
	}
	return ans * sign;
}
