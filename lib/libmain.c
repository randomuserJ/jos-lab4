// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, uvpd, and uvpt.

#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	// LAB 3: Your code here.

	// aby sme mohli inicializovat smernik thisenv na aktualne prostredie z pola envs
	// potrebujem zistit, o ake prostredie sa jedna cez sys_getenvid()

	// env.h - environment id je 32 bit hodnota, pricom index do pola envs
	// tvori spodnych 10 bitov (makro ENVX)
	thisenv = &envs[ENVX(sys_getenvid())];

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

