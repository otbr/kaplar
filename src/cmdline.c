#include "cmdline.h"

#include <stdio.h>
#include <string.h>

static int	l_argc;
static char	**l_argv;

void cmdl_init(int argc, char **argv)
{
	l_argc = argc;
	l_argv = argv;
}

const char *cmdl_get_string(const char *cmd)
{
	long i, aux;
	const char *str;

	// start from index 1 as index 0 is the
	// system path to the executable
	for(i = 1; i < l_argc; ++i){
		str = l_argv[i];
		aux = (long)strlen(cmd);
		if(aux <= strlen(str) && strncmp(cmd, str, aux) == 0)
			return l_argv[i];
	}

	return NULL;
}

int cmdl_get_long(const char *cmd, long *value)
{
	const char *str, *sstr;

	str = cmdl_get_string(cmd);
	if(str != NULL){
		for(sstr = str;
			*sstr != '=' && *sstr != 0x00;
			++sstr);

		// only return true if there is actually
		// an integer to return
		if(*sstr == '=' && sscanf(sstr + 1, "%ld", value) == 1)
			return 0;
	}
	return -1;
}

int cmdl_get_float(const char *cmd, float *value)
{
	const char *str, *sstr;

	str = cmdl_get_string(cmd);
	if(str != NULL){
		for(sstr = str;
			*sstr != '=' && *sstr != 0x00;
			++sstr);

		// only return true if there is actually
		// a float to return
		if(*sstr == '=' && sscanf(sstr + 1, "%f", value) == 1)
			return 0;
	}
	return -1;
}
