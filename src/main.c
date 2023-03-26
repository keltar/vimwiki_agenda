#include <stdio.h>
#include "vimwiki.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "output.h"

#define DEFAULT_WIKI_PATHS 8
#define DEFAULT_AGENDA_DAYS 7

static char *resolve_path(const char *p) {
	if(!p) { return NULL; }
	size_t len = 0;
	const char *home = NULL;
	if(p[0] == '~') {
		home = getenv("HOME");
		len += strlen(home);
		if(len > 0) { len--; }
	}
	len += strlen(p);

	char *r = malloc(len+1);
	char *t = r;
	if(home) {
		t = stpcpy(r, home);
		p++;
	}
	strcpy(t, p);

	char *ret = realpath(r, NULL);
	free(r);

	return ret;
}

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static int notes_sort_by_timestamp(const void *a, const void *b) {
	const vimwiki_note *note_a = (const vimwiki_note*)a;
	const vimwiki_note *note_b = (const vimwiki_note*)b;
	const uint64_t ta = MAX(note_a->timestamp_scheduled, note_a->timestamp_deadline);
	const uint64_t tb = MAX(note_b->timestamp_scheduled, note_b->timestamp_deadline);
	int64_t tdiff = (int64_t)ta - (int64_t)tb;
	if(tdiff == 0) {
		return 0;
	} else if(tdiff < 0) {
		return -1;
	} else {
		return 1;
	}
}

static void print_usage(const char *argv0) {
	printf("Usage: %s [days]\n", argv0);
	printf("\n\tdays: number of days (in future) to show agenda for; default is %d\n\n",
			DEFAULT_AGENDA_DAYS);
}


int main(int argc, char **argv) {
	vimwiki_path wiki_paths_buffer[DEFAULT_WIKI_PATHS];
	vimwiki_path *wiki_paths = wiki_paths_buffer;
	size_t num_wiki_paths = vimwiki_query_wiki_paths(wiki_paths, DEFAULT_WIKI_PATHS);
	// allocate bigger array if default is too small
	if(num_wiki_paths > DEFAULT_WIKI_PATHS) {
		wiki_paths = malloc(sizeof(*wiki_paths) * num_wiki_paths);
		num_wiki_paths = vimwiki_query_wiki_paths(wiki_paths, num_wiki_paths);
	}

	int lookup_days = DEFAULT_AGENDA_DAYS;
	if(argc > 1) {
		if(argc == 2) {
			char *end = NULL;
			long v = strtol(argv[1], &end, 10);
			if(end && end[0]) {
				print_usage(argv[0]);
				return 0;
			}
			lookup_days = (int)v;
		} else {
			print_usage(argv[0]);
			return 0;
		}
	}

	uint64_t lookup_timestmap = time(NULL) + 60*60*24*lookup_days;

	// load each wiki
	for(size_t wiki_i = 0; wiki_i != num_wiki_paths; ++wiki_i) {
		char *resolved_path = resolve_path(wiki_paths[wiki_i].path);
		if(!resolved_path) {
			fprintf(stderr, "can't resolve path \"%s\"\n", wiki_paths[wiki_i].path);
			continue;
		}

		//printf("loading wiki %s\n", resolved_path);

		vimwiki *vw = vimwiki_load(resolved_path, wiki_paths[wiki_i].ext, wiki_paths[wiki_i].syntax);

		size_t num_notes = 0;
		vimwiki_note *notes = vimwiki_load_notes(vw, &num_notes, VIMWIKI_NOTES_LOAD_ALL_TIMESTAMPED);

		// sort by scheduled or deadline
		qsort(notes, num_notes, sizeof(*notes), &notes_sort_by_timestamp);

		print_notes(NULL, notes, num_notes, lookup_timestmap);

		vimwiki_destroy_notes(vw, notes, num_notes);
		vimwiki_destroy(vw);
		free(resolved_path);
	}

	// clear wiki_paths array if it isn't preallocated
	if(wiki_paths != wiki_paths_buffer) {
		free(wiki_paths);
	}

	return 0;
}
