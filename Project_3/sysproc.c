#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "processInfo.h"

struct{
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int //1-1
sys_hello(void)
{
	cprintf("helloxv6\n");
	return 0;
}

int //1-2
sys_hello_name(void)
{
	char *name;
	if(argstr(0,&name)<0)
		return -1;
	cprintf("hello %s\n",name);
	return 0;
}
	
int //1-3
sys_get_num_proc(void)
{
	struct proc *p;
	int ct=0;

	acquire(&ptable.lock);
	for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
		if(p->state!=UNUSED) //RUNNNG , RUNNABLE , SLEEPING , ZOMBIE 상태의 프로세스 수를 파악
			ct++;
	}
	release(&ptable.lock);
	return ct;
}

int //1-4
sys_get_max_pid(void)
{
	struct proc *p;
	int max=0;
	
	acquire(&ptable.lock);
	for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
		if(p->pid>max)
			max=p->pid;
	}
	release(&ptable.lock);
	return max;
}

int //1-5
sys_get_proc_info(void)
{
	struct proc *p;
	struct processInfo *procInfo;
	int pid;
	int fail=1;
	
	if(argint(0,&pid)<0) //pid와 processInfo 구조체를 인자로 입력받음
		return -1;
	if(argptr(1,(void*)&procInfo,sizeof(*procInfo))<0)
		return -1;
	
	acquire(&ptable.lock);
	for(p=ptable.proc;p<&ptable.proc[NPROC];p++){ //pid 탐색
		if(p->pid==pid){
			fail=0;
			break;
		}
	}
	if(fail||p->state==UNUSED){ //입력받은 pid가 존재하지 않는 경우 혹은 활성화되지 않은 경우 -1 리턴
		release(&ptable.lock);
		return -1;
	}

	procInfo->pid=pid; //pid를 저장
	if(pid==1)
		procInfo->ppid=0;
	else
		procInfo->ppid=p->parent->pid; //부모 프로세스의 pid로 ppid를 저장

	procInfo->psize=p->sz; //프로세스의 크기를 저장
	procInfo->numberContextSwitches=p->ctxt_count; //context switch 횟수를 저장
	release(&ptable.lock);
	return 0;
}

int //2-1
sys_set_prio(void)
{
	int n;
	if(argint(0,&n)<0)
		return -1;

	myproc()->priority=n; //현재 프로세스의 우선순위를 설정
	if(myproc()->priority!=n)
		return -1;
	return 0;
}

int //2-2
sys_get_prio(void)
{
	int prio;

	prio=myproc()->priority; //현재 프로세스의 우선순위를 저장
	if(prio<=0) //우선순위는 양의 정수이므로 범위를 벗어나면 -1 리턴
		return -1;
	return prio;
}
