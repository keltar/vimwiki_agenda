/*
 */

#ifndef _hVIMWIKI_H
#define _hVIMWIKI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VIMWIKI_PATH_MAX 4096
#define VIMWIKI_SHORT_ARRAY_SIZE 5

typedef struct vimwiki vimwiki;

typedef struct vimwiki_note {
	int idx;

	bool completed;				// true if [X]

	const char *filename;		// reference into string list; shared
	int line;

	char *text;

	// all times are in seconds; timestamps are UNIX time
	uint64_t timestamp_scheduled;
	uint64_t timestamp_deadline;
	/*uint32_t duration;
	struct {
		uint32_t num_reminders;
		uint32_t short_array_reminders[VIMWIKI_SHORT_ARRAY_SIZE];
		uint32_t *reminders;	// points to short_array_reminders if short
	} reminders;*/
} vimwiki_note;

enum vimwiki_syntax_e {
	VIMWIKI_SYNTAX_DEFAULT,
	VIMWIKI_SYNTAX_MARKDOWN,

	VIMWIKI_SYNTAX_UNKNOWN
};

// notes load flags
enum {
	VIMWIKI_NOTES_LOAD_LISTS_TIMESTAMPED = 1,
	VIMWIKI_NOTES_LOAD_LISTS = 1 << 1,

	VIMWIKI_NOTES_LOAD_ALL_TIMESTAMPED =
		VIMWIKI_NOTES_LOAD_LISTS_TIMESTAMPED,
	VIMWIKI_NOTES_LOAD_ALL = ~0u,
};

// path/ext/syntax for each vimwiki; defined in vim via g:vimwiki_list
typedef struct vimwiki_path {
	char path[VIMWIKI_PATH_MAX];
	char ext[32];
	enum vimwiki_syntax_e syntax;
} vimwiki_path;

/* Load wiki list (by querying vim). Store in given array, max w_size elements.
 * Returns required max_w */
size_t vimwiki_query_wiki_paths(vimwiki_path *w, size_t max_w);

vimwiki *vimwiki_load(const char *path, const char *ext, enum vimwiki_syntax_e syntax);
void vimwiki_destroy(vimwiki *vw);

vimwiki_note *vimwiki_load_notes(vimwiki *vw, size_t *num_notes, uint32_t flags);

// size must be the same as returned by vimwiki_load_notes
void vimwiki_destroy_notes(vimwiki *vw, vimwiki_note *notes, size_t num_notes);

const char *vimwiki_str_to_timestamp(const char *str, uint64_t *timestamp);
void vimwiki_str_from_timestamp(char *s, size_t sz, uint64_t timestamp);

#endif
