#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.

void
sched_yield(void)
{
	struct Env *idle = NULL;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.
	// LAB 4: Your code here.

	int index, first = 0;

	// ID aktualneho prostredia nam hovori, ktore bezalo ako posledne
	// ak este ziadne nebezalo, index do pola je nula
	// inak je index zapisany v spodnych 10 bitov env_id (ENVX makro)
	// bez +1 to tiez ide, ale obcas nie uplne spravne
	if (curenv)
		first = ENVX(curenv->env_id)+1;

	// prechadzame ceze vsetky (NENV) prostredia a hladame,
	// ktore z nich je spustitelne
	for(int i = 0; i < NENV; i++){
		index = (first+i) % NENV;
		if (envs[index].env_status == ENV_RUNNABLE){
			idle = &envs[index];
			break;
		}
	}
	
	// ak sme nasli spustitelne prostredie, spustime ho
	// ak sme nenasli, ale posledne ma stale status ENV_RNNING,
//	// vyberieme to (v env_run sa ukonci a spusti znovu)
	if (idle)
		env_run(idle);
	else{
		if (curenv && curenv->env_status == ENV_RUNNING)
			env_run(curenv);
	}

	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		// Uncomment the following line after completing exercise 13
		"sti\n"		// prerusenia na ostatnych procesoroch
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

