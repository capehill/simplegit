/**
 * @file
 *
 * This is a main entry for the simple git tool.
 */

#include "git.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <git2.h>

#include "cmds.h"
#include "common.h"
#include "errors.h"
#include "help.h"
#include "strbuf.h"
#include "version.h"

#ifdef HAVE_SGIT_VERSION_H
#include "sgit-version.h"
#endif

static char *argv0_path;

static char *git_extract_argv0_path(char *argv0)
{
	char *slash;

	if (!argv0 || !*argv0)
		return NULL;
	slash = argv0 + strlen(argv0);

	while (argv0 <= slash && !is_dir_sep(*slash))
		slash--;

	if (slash >= argv0)
	{
		argv0_path = strndup(argv0, slash - argv0);
		return slash + 1;
	}

	return argv0;
}

#if defined (AMIGA)
__attribute__((unused)) static const char version_tag[] =
	"$VER: sgit 0." SIMPLEGIT_REVISION_STRING " (" SIMPLEGIT_DATE_STRING ")";
#endif

#if defined (USE_AMISSL)

#include <libraries/amisslmaster.h>

#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <proto/exec.h>

struct Library *AmiSSLMasterBase;
struct AmiSSLMasterIFace *IAmiSSLMaster;

struct AmiSSLIFace *IAmiSSL;
struct Library *AmiSSLBase;

static void cleanup_amissl(void)
{
	if (IAmiSSL)
	{
		IExec->DropInterface((struct Interface*)IAmiSSL);
		IAmiSSLMaster->CloseAmiSSL();
	}
	if (IAmiSSLMaster)
	{
		IExec->DropInterface((struct Interface*)IAmiSSLMaster);
		IExec->CloseLibrary(AmiSSLMasterBase);
	}
}

static int init_amissl(void)
{
	if (!(AmiSSLMasterBase = IExec->OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION)))
		goto out;

	if (!(IAmiSSLMaster = (struct AmiSSLMasterIFace *)IExec->GetInterface(AmiSSLMasterBase, "main", 0, NULL)))
		goto out;

	if (!IAmiSSLMaster->InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE))
		goto out;

	if (!(AmiSSLBase = IAmiSSLMaster->OpenAmiSSL()))
		goto out;

	if (!(IAmiSSL = (struct AmiSSLIFace *)IExec->GetInterface(AmiSSLBase, "main", 0, NULL)))
		goto out;

	atexit(cleanup_amissl);
	return 1;
out:
	cleanup_amissl();
	return 0;
}


#endif

/**
 * Main entry.
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char **argv)
{
	git_repository *repo = NULL;
	char *cmd;
	int rc = EXIT_FAILURE;
	int err;
	int git_libgit2_initialied_err;

#if defined (USE_AMISSL)
	if (!init_amissl())
	{
		fprintf(stderr, "The amissl initialization failed!\n");
		git_libgit2_initialied_err = 1; /* don't call git_libgit2_shutdown() */
		goto out;
	}
#endif

	if ((git_libgit2_initialied_err = git_libgit2_init()) < 0)
	{
		fprintf(stderr,"The libgit2 initialization failed!\n");
		goto out;
	}
	git_libgit2_initialied_err = 0;

	cmd = git_extract_argv0_path(argv[0]);

	if (!prefixcmp(cmd,"git-"))
	{
		cmd += 4;
		argv[0] = cmd;
	} else
	{
		int i;

		argc--;
		argv++;

		for (i=0;i<argc;i++)
		{
			if (argv[0][0] != '-') break;

			if (!strcmp("--exec-path",argv[i]))
			{
				printf(argv0_path);
				rc = EXIT_SUCCESS;
				goto out;
			}
		}
	}

	if (argc <= 0)
	{
		cmd_help(NULL, 1, &cmd);
		goto out;
	}

	if (!strcmp(argv[0], "help"))
	{
		cmd_help(NULL, 1, &cmd);
		rc = EXIT_SUCCESS;
		goto out;
	}

	if (!strcmp(argv[0], "version"))
	{
		cmd_version(NULL, 0, NULL);
		rc = EXIT_SUCCESS;
		goto out;
	}

	err = git_repository_open_ext(&repo, ".", 0, NULL);
	if (err < 0)
	{
		/* Does the command require a repo? Check against those that do not
		 * require one */
		if (strcmp(argv[0], "init") && strcmp(argv[0], "config") && strcmp(argv[0], "clone"))
		{
			libgit_error();
			goto out;
		}

		repo = NULL;
	}

	git_cb handler = lookup_handler(argv[0]);
	if (handler != NULL)
	{
		rc = handler(repo, argc, argv);
	} else
	{
		fprintf(stderr,"Command \"%s\" not supported\n",argv[0]);
		rc = 20;
	}
out:
	git_repository_free(repo);
	if (!git_libgit2_initialied_err)
		git_libgit2_shutdown();
	return rc;
}
