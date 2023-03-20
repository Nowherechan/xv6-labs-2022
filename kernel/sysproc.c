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
  backtrace(); // For lab-4 Backtrace
  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
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
sys_sigalarm(void)
{
  int ticks;
  uint64 handler;
  argint(0, &ticks);
  argaddr(1, &handler);

  struct proc *p = myproc();
  if (!(handler || ticks)) {
    p->alarm_enabled = 0;
  } else {
    p->alarm_enabled = 1;
  }
  p->alarm_handler = (void*)handler;
  p->alarm_interval = ticks;
  p->ticks_passed = 0;
  p->alarm_processing = 0;
  return 0;
}

uint64
sys_sigreturn(void)
{ 
  struct proc *p = myproc();
  // First 5 register are associated with kernel (except epc) so they should not be restored.
  // 5 * 8 (64 bits = 8 bytes) == 40
  // memmove((void*)(p->trapframe)+40, (void*)(p->trapframe_bak)+40, sizeof(struct trapframe)-40);
  // p->trapframe->epc = p->trapframe_bak->epc;
  // Actually these 4 kernel_* entries don't matter. So just copy all the trapframe.
  *(p->trapframe) = *(p->trapframe_bak);
  p->alarm_processing = 0;  
  return p->trapframe->a0;
}