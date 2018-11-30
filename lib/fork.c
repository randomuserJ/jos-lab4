#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// kontrola, ci chybový kód obsahuje príznak 'FEC_WR' - chyba zapisu
	// a ci je prislusny zaznam stranky oznaceny ako PTE_COW
//	if (!(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW))
//		panic("not copy-on-write");

	if (!(
			(err & FEC_WR) && (uvpd[PDX(addr)] & PTE_P) && 
			(uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW)))
		panic("not copy-on-write");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	// alokujeme novu stranku v pamati 
	// a namapujeme ju na docasne miesto vo VAP (PFTEMP (komentare))
	// skopirujeme tam obsah stranky, ktora vypadok vyvolala
	addr = ROUNDDOWN(addr, PGSIZE);
	if (sys_page_alloc(0, PFTEMP, PTE_W|PTE_U|PTE_P))
		panic("sys_page_alloc");

	// kopirovanie pamati (kam, odkial, kolko)
	memcpy(PFTEMP, addr, PGSIZE);

	if (sys_page_map(0, PFTEMP, 0, addr, PTE_W|PTE_U|PTE_P))
		panic("sys_page_map");

	if (sys_page_unmap(0, PFTEMP))
		panic("sys_page_unmap");

	return;
	panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.

	// chceme napovat stranku do adresneho priestora potomka ako PTE_COW
	// a premapovat stranku do adresneho priestora rodica ako PTE_COW
	// 	najdeme si adresu nasej stranky
	// 	ak je stranka W alebo COW, namapujeme ju tiez ako COW
	//	ak nie, tak mapujeme ako nezapisovatelne
	void *addr = (void*)(pn*PGSIZE);
	if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P);
		if (r)
			return r;
		r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P);
		if (r)
			return r;
	}
	else
		sys_page_map(0, addr, envid, addr, PTE_U|PTE_P);

	return 0;
	panic("duppage not implemented");
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

	// nastavime obsluhu vypadkov stranok na funkciu pgfault
	set_pgfault_handler(pgfault);
	
	envid_t envid;
	uint32_t addr;

	// Allocate a new child environment.
	// The kernel will initialize it with a copy of our register state,
	// so that the child will appear to have called sys_exofork() too -
	// except that in the child, this "fake" call to sys_exofork()
	// will return 0 instead of the envid of the child.
	envid = sys_exofork();	// vytvorime potomka 

	// ak sys_exofork vrati chybu, vratime ju tiez
	// ak vrati 0, je to dieta a nastavime mu spravny thisenv 
	// 	dieta nevie svoje id, kedze envid = 0
//	cprintf("envid: %x\n", envid);
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

//	cprintf("parent\n");
	// We're the parent.
	// Eagerly copy our entire address space into the child.
	// This is NOT what you should do in your fork implementation.

// zatial bolo vsetko rovnake (skopirovane)
// az teraz nastava zmena oproti dumbforku
// nekopirujeme cely adresny priestor (stranky), ale iba ich mapovanie 

// PGNUM nam vrati cislo stranky (UTOP/PGSIZE alebo UTOP >> PTXSHIFT)
// ak narazime na zasobnik UXSTACKTOP, chceme ho preskocit
// ak stranka nie je pritomna (kontrola cez uvpd), preskakujeme ju

	for (addr = 0; addr < USTACKTOP; addr += PGSIZE)
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)
			&& (uvpt[PGNUM(addr)] & PTE_U)) {

		duppage(envid, PGNUM(addr));
	}
/*	
	for (int pn = 0; pn < PGNUM(UTOP); pn++){
		if (pn == PGNUM(UXSTACKTOP-PGSIZE))
			continue;
		// if pn not present in uvpd 
		//	continue;
		duppage(envid, pn);
	}
*/
	// namapujeme stranku pre dieta
	int r = sys_page_alloc(envid, (void*)(UXSTACKTOP-PGSIZE), PTE_W|PTE_U|PTE_P);
	if (r)
		return r;
	
	// nastavime vstupny bod obsluhy vynimiek
	// treba ju nalinkovat, lebo je assemblerovska
	extern void _pgfault_upcall();
	sys_env_set_pgfault_upcall(envid, (void*)_pgfault_upcall);


	// Start the child environment running:
	// 	nastavime potomka na RUNNABLE 
	// 	o zvysok sa postara planovac
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)))
		panic("sys_env_set_status: %e", r);

	return envid;

	panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
