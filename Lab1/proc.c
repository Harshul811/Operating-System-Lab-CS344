#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;

int fork(void )
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf; //trapeframe

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);

  np->cwd = idup(proc->cwd);

  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

int thread_create(void(*fcn)(void*), void *arg, void *stack)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  np->sz = curproc->sz;
  np->parent = curproc;

  np->pgdir = curproc->pgdir;
  *np->tf = *curproc->tf;
  np->is_thread = 1;

  np->tf->eax = 0;

  np->tf->eip = (uint)fcn; // index pointer

  np->tf->esp = (uint)stack + 4092; //stack pointer
  *((uint *)(np->tf->esp)) = (uint)arg;
  *((uint *)(np->tf->esp - 4)) = 0xFFFFFFFF;
  np->tf->esp = np->tf->esp - 4;

  for(i = 0; i < NOFILE; i++)
  {
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  }

  np->cwd = idup(curproc->cwd);
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  pid = np->pid;
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  return pid;
}

int thread_join(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  while(true)
  {
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc || p->is_thread != 1)
        continue;

      havekids = 1;
      if (p->state == ZOMBIE)
      {
        pid = p->pid;
        return pid;
      }
    }

    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }
    sleep(curproc, &ptable.lock);
  }
}



