#include "help.h"

#include <stdio.h>
#include <stdlib.h>

#include <git.h>

#include "cmds.h"

int cmd_help(git_repository *repo, int argc, char **argv)
{
	(void)repo;
	(void)argc;

	fprintf(stdout,"Usage: %s <command>\n", argv[0]);
	print_commands_overview();
	return EXIT_SUCCESS;
}
