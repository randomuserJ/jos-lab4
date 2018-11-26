// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display some shit", mon_backtrace},
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// jos/kern/kdebug.h 
	struct Eipdebuginfo eip_info;

	// (inc/x86.h) Funkcia na zistenie EBP (vracia int32)
	uint32_t ebp = read_ebp();
	uint32_t* ebpp = NULL;
	uint32_t eip = (uint32_t)mon_backtrace;
	uint32_t args[5];
	int i;

	while(ebp != 0) {
		
		ebpp = (uint32_t*)ebp;
		
		// nacitanie argumentov	
		// premenne su v zasobniku o jedno vyssie, nez navratova adresa (+2) 
		for (i=0; i<5; i++){
			args[i] = *(ebpp+i+2);	
		}
		cprintf("ebp %x eip %x args %08x %08x %08x %08x %08x\n",
			ebp, eip, args[0], args[1], args[2], args[3], args[4]); 

		// debuginfo_eip vrati 0, ak bola adresa eip spravna
		// identifikuje funkciu, kde ukazuje eip a ulozi ju do eip_info
		if (debuginfo_eip(eip, &eip_info) == 0){
			// eip_file - nazov file v ktorom je eip
			// eip_line - cislo riadku kodu
			// eip_fn_namelen - dlzka nazvu funkcie z eip (hodnota pre *)
			// eip_fn_name - nazov funkcie (dlzka vypisu prisposobena)
			// eip_fn_addr - adresa zaciatku funkcie 
			cprintf("%s:%d: %.*s+%d\n\n", eip_info.eip_file, eip_info.eip_line,
					eip_info.eip_fn_namelen, eip_info.eip_fn_name, eip - eip_info.eip_fn_addr);
				
		}
		// eip = ebp pointer + 1 (podla obrazka stacku)
		eip = *(ebpp+1); 
		// v ebpp sa nachadza adresa predosleho ebp 
		ebp = *ebpp;	
	}
	
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
