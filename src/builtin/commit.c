#include "commit.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <git2.h>

#include "common.h"
#include "date.h"
#include "environment.h"
#include "errors.h"
#include "git-support.h"
#include "git.h"
#include "strbuf.h"

struct mergehead_peel_payload
{
	git_repository *repo;
	git_commit **next_parent;
};

static int mergehead_peel_cb(const git_oid *oid, void *payload)
{
	int err;
	struct mergehead_peel_payload *peel_payload = (struct mergehead_peel_payload*)payload;

	if ((err = git_commit_lookup(peel_payload->next_parent, peel_payload->repo, oid)))
		return -1;
	peel_payload->next_parent++;
	return 0;
}

int cmd_commit(git_repository *repo, int argc, char **argv)
{
	int err = 0;
	git_reference *head = NULL;
	git_reference *branch = NULL;
	git_commit *parent = NULL;
	git_oid tree_oid;
	git_oid commit_oid;
	git_commit *commit = NULL;

	git_index *idx = NULL;

	git_status_list *status_list = NULL;
	git_status_options status_opts = GIT_STATUS_OPTIONS_INIT;

	git_signature *author_signature = NULL;
	git_signature *committer_signature = NULL;
	git_tree *tree = NULL;
	git_commit **parents = NULL;
	const char *message = NULL;

	int i;
	int rc = EXIT_FAILURE;
	int num_parents = 0;

	int allow_empty = 0;
	int amend = 0;
	int reset_author = 0;

	for (i=1;i<argc;i++)
	{
		if (!strcmp(argv[i],"-m"))
		{
			if (++i < argc)
			{
				message = argv[i];
			} else
			{
				fprintf(stderr,"Option -m misses argument\n");
				goto out;
			}
		}
		else if (!strcmp(argv[i], "--allow-empty"))
		{
			allow_empty = 1;
		}
		else if (!strcmp(argv[i], "--amend"))
		{
			amend = 1;
		}
		else if (!strcmp(argv[i], "--reset-author"))
		{
			reset_author = 1;
		}
		else if (argv[i][0] == '-')
		{
			fprintf(stderr,"Unknown option \"%s\"\n",argv[i]);
			goto out;
		}
	}

	if (!message)
	{
		fprintf(stderr,"A commit message is required (-m option)!\n");
		goto out;
	}

	/* Count the number of parents */
	if ((git_repository_head(&head,repo)) == GIT_OK)
		num_parents++;
	err = sgit_repository_mergeheads_count(&num_parents, repo);
	if (err && err != GIT_ENOTFOUND)
		goto out;

	/* Now determine the actual parents */
	if (num_parents)
	{
		if (!(parents = malloc(sizeof(*parents)*num_parents)))
		{
			fprintf(stderr,"Not enough memory!\n");
			goto out;
		}

		if ((err = git_reference_peel((git_object**)&parent,head,GIT_OBJ_COMMIT)))
			goto out;
		parents[0] = parent;

		if (num_parents > 1)
		{
			struct mergehead_peel_payload peel_payload;

			peel_payload.repo = repo;
			peel_payload.next_parent = parents + 1;

			if ((err = git_repository_mergehead_foreach(repo, mergehead_peel_cb, &peel_payload)))
				goto out;
		}
	}

	if ((err = sgit_get_author_signature(repo, &author_signature)) != GIT_OK)
		goto out;

	if ((err = sgit_get_committer_signature(repo, &committer_signature)) != GIT_OK)
		goto out;

	/* Write index as tree */
	if ((err = git_repository_index(&idx,repo)) != GIT_OK)
		goto out;

	if ((err = git_status_list_new(&status_list, repo, &status_opts)))
		goto out;

	if (!allow_empty && git_status_list_entrycount(status_list) == 0)
	{
		fprintf(stderr,"Nothing to commit! Use --allow empty for an empty commit!\n");
		goto out;
	}
	if ((err = git_index_write_tree_to(&tree_oid, idx, repo)) != GIT_OK)
		goto out;
	if ((err = git_tree_lookup(&tree,repo,&tree_oid)) != GIT_OK)
		goto out;

	/* Write tree as commit */
	if (!amend)
	{
		if ((err = git_commit_create(&commit_oid, repo, "HEAD", author_signature, committer_signature,
					NULL, message, tree, num_parents, (const git_commit**)parents)) != GIT_OK)
			goto out;
	} else
	{
		if (num_parents != 1 || parent == NULL)
		{
			fprintf(stderr, "Amending works only for a single parent!\n");
			goto out;
		}

		if ((err = git_commit_amend(&commit_oid, parent, "HEAD",
					reset_author?author_signature:NULL, reset_author?committer_signature:NULL, NULL, message, tree)))
			goto out;
	}

	rc = EXIT_SUCCESS;
out:
	if (err) libgit_error();
	if (head) git_reference_free(head);
	if (tree) git_tree_free(tree);
	if (status_list) git_status_list_free(status_list);
	if (idx) git_index_free(idx);
	if (parents)
	{
		for (i=0;i<num_parents;i++)
			git_commit_free(parents[i]);
		free(parents);
	}
	if (author_signature) git_signature_free(author_signature);
	if (committer_signature) git_signature_free(committer_signature);
	if (commit) git_commit_free(commit);
	if (branch) git_reference_free(branch);
	return rc;
}
