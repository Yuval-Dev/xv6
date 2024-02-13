#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

	uint64
sys_exit(void)
{
	int n;
	argint(0, &n);
	exit(n);
	return 0;  // not reached
}

	uint64
sys_getpid(void)
{
	return myproc()->pid;
}

	uint64
sys_fork(void)
{
	return fork();
}

	uint64
sys_wait(void)
{
	uint64 p;
	argaddr(0, &p);
	return wait(p);
}

	uint64
sys_sbrk(void)
{
	uint64 addr;
	int n;

	argint(0, &n);
	addr = myproc()->sz;
	if(growproc(n) < 0)
		return -1;
	return addr;
}

	uint64
sys_sleep(void)
{
	int n;
	uint ticks0;

	argint(0, &n);
	if(n < 0)
		n = 0;
	acquire(&tickslock);
	ticks0 = ticks;
	backtrace();
	while(ticks - ticks0 < n){
		if(killed(myproc())){
			release(&tickslock);
			return -1;
		}
		sleep(&ticks, &tickslock);
	}
	release(&tickslock);
	return 0;
}

	uint64
sys_kill(void)
{
	int pid;

	argint(0, &pid);
	return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
	uint64
sys_uptime(void)
{
	uint xticks;

	acquire(&tickslock);
	xticks = ticks;
	release(&tickslock);
	return xticks;
}

uint64
sys_sigalarm(void) {
	printf("alarm!\n");
	int tick_rate;
	int handler;
	argint(0, &tick_rate);
	argint(1, &handler);
	struct proc * p = myproc();
	p->tick_rate = tick_rate;
	p->alarm_handler = (uint64)handler;
	p->rem_ticks = tick_rate;
	return 0;
}

uint64
sys_sigreturn(void) {
	struct proc *p = myproc();
	p->rem_ticks = p->tick_rate;
	*p->trapframe = *p->saved_trapframe;
	return p->trapframe->a0;
}
