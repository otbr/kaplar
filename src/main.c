#include "cmdline.h"
#include "work.h"
#include "scheduler.h"
#include "network.h"

#include <stdio.h>

static void print_hello(void *unused)
{
	printf("Hello World!\n");
}

int main(int argc, char **argv)
{
	cmdl_init(argc, argv);
	work_init();
	scheduler_init();
	for(int i = 1; i < 10; i++)
		scheduler_add(i * 1000, print_hello, NULL);
	getchar();
	net_init();
	getchar();
	return 0;
}