/*
 */

// FIXME move to CMakeLists.txt
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#include "vimwiki.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <dirent.h>
#include <time.h>
#include "json.h"

#define VIMWIKI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define VIMWIKI_MIN_ARRAY_CAPACITY 16

static void *vimwiki_realloc(vimwiki *vw, void *ptr, size_t size);

/* packed c-strings, each strings starts right after previous one.
 * After last string there is extra '\0' byte. */
typedef struct {
	size_t byte_length;
	size_t num_strings;
	const char *str;
} vimwiki_string_list;

static bool vimwiki_string_list_append(vimwiki *vw,
		vimwiki_string_list *sl, size_t *capacity,
		const char *str) {
	const size_t str_len = strlen(str);
	const size_t current_capacity = *capacity;
	const size_t current_len = sl->byte_length;
	if(current_len + str_len >= current_capacity) {
		// resize
		const size_t new_capacity = 
				VIMWIKI_MAX(current_capacity*2, current_capacity+str_len+1);
		char *new_str = vimwiki_realloc(vw, (void*)sl->str, new_capacity);
		if(!new_str) {
			return false;
		}
		*capacity = new_capacity;
		sl->str = new_str;
	}

	// append
	memcpy((char*)(sl->str)+current_len, str, str_len+1);
	sl->byte_length += str_len+1;
	sl->num_strings++;
	return true;
}


struct vimwiki {
	// wiki root directory
	char wiki_root[VIMWIKI_PATH_MAX];
	size_t wiki_root_length;

	vimwiki_string_list file_list;
};

static void *vimwiki_alloc(vimwiki *vw, size_t size) {
	(void)vw;
	return malloc(size);
}

static void vimwiki_free(vimwiki *vw, void *ptr) {
	(void)vw;
	free(ptr);
}

static void *vimwiki_realloc(vimwiki *vw, void *ptr, size_t size) {
	(void)vw;
	return realloc(ptr, size);
}

static char *vimwiki_slurp(vimwiki *vw, const char *path, size_t *size) {
	FILE *file = fopen(path, "rb");
	if(!file) {
		fprintf(stderr, "can't open %s file: %s\n",
				path, strerror(errno));
		return NULL;
	}

	char *ret = NULL;
	if(fseek(file, 0, SEEK_END) != 0) {
		fprintf(stderr, "%s fseek failed: %s\n", path, strerror(errno));
		fclose(file);
		return NULL;
	}
	const long l = ftell(file);
	if(l < 0) {
		fprintf(stderr, "%s ftell failed: %s\n", path, strerror(errno));
		fclose(file);
		return NULL;
	}
	if(fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "%s fseek failed: %s\n", path, strerror(errno));
		fclose(file);
		return NULL;
	}

	ret = vimwiki_alloc(vw, l+1);
	if(fread(ret, 1, l, file) != (size_t)l) {
		fprintf(stderr, "%s fread failed: %s\n", path, strerror(errno));
		fclose(file);
		vimwiki_free(vw, ret);
		return NULL;
	}
	// add tailing '\0' for string handling
	ret[l] = '\0';

	fclose(file);

	if(size) {
		*size = l;
	}
	return ret;
}

static size_t vimwiki_strlcpy(char *out, const char *in, size_t out_sz) {
	const size_t in_size = strlen(in);
	if(out_sz == 0) { return in_size; }
	size_t copy_size = (out_sz-1 < in_size) ? out_sz-1 : in_size;
	memcpy(out, in, copy_size);
	out[copy_size] = '\0';
	return in_size;
}

static bool vimwiki_str_ends_with(const char *str, const char *ext, size_t ext_len) {
	const size_t str_len = strlen(str);
	if(str_len < ext_len) { return false; }
	return memcmp(str+str_len-ext_len, ext, ext_len) == 0;
}

/*static bool vimwiki_array_append(vimwiki *vw, void **array, void *value,
		size_t element_size, size_t *length, size_t *capacity) {
	size_t cap = *capacity;
	size_t len = *length;
	if(len + 1 <= cap) {
		cap = VIMWIKI_MAX(cap*2, VIMWIKI_MIN_ARRAY_CAPACITY);
		void *new_array = vimwiki_realloc(vw, *array, cap*element_size);
		if(!new_array) { return false; }
		*capacity = cap;
		*array = new_array;
	}
	memcpy(((char*)*array) + element_size*len, value, element_size);
	(*length)++;
	return true;
}*/

static enum vimwiki_syntax_e vimwiki_syntax_from_str(const char *str) {
	if(strcmp(str, "default") == 0) {
		return VIMWIKI_SYNTAX_DEFAULT;
	} else if(strcmp(str, "markdown") == 0) {
		return VIMWIKI_SYNTAX_MARKDOWN;
	}
	return VIMWIKI_SYNTAX_UNKNOWN;
}

static bool vimwiki_add_to_file_list(vimwiki *vw, const char *path,
		const char *ext, size_t ext_len,
		vimwiki_string_list *sl, size_t *capacity) {
	DIR *dir = opendir(path);
	if(!dir) {
		return false;
	}

	struct dirent *de;
	while((de = readdir(dir))) {
		const int dtype = de->d_type;
		// skip dirs . and ..
		if(dtype == DT_DIR && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)) {
			continue;
		}
		// skip hidden
		if(de->d_name[0] == '.') {
			continue;
		}
		// skip regular files with non-matching ext
		if(dtype == DT_REG && !vimwiki_str_ends_with(de->d_name, ext, ext_len)) {
			continue;
		}

		// generate path/d_name subpath
		char subpath[VIMWIKI_PATH_MAX];
		if(dtype == DT_REG || dtype == DT_DIR) {
			size_t subpath_size = snprintf(subpath, sizeof(subpath), "%s/%s", path, de->d_name);
			assert(subpath_size <= sizeof(subpath));
		}

		if(dtype == DT_REG) {
			// add file to list
			if(!vimwiki_string_list_append(vw, sl, capacity, subpath+vw->wiki_root_length)) {
				return false;
			}
		} else if(dtype == DT_DIR) {
			if(!vimwiki_add_to_file_list(vw, subpath, ext, ext_len, sl, capacity)) {
				return false;
			}
		}
		//printf("%s (%d, %d, %d)\n", de->d_name, de->d_type, DT_DIR, DT_REG);
	}

	closedir(dir);
	return true;
}

// get recursive file listing for specified path
static vimwiki_string_list vimwiki_get_file_list(vimwiki *vw, const char *path, const char *ext) {
	const size_t ext_len = strlen(ext);
	vimwiki_string_list l = {};
	size_t capacity = VIMWIKI_PATH_MAX;
	l.str = vimwiki_alloc(vw, capacity);
	if(!capacity) {
		return l;
	}

	if(vimwiki_add_to_file_list(vw, path, ext, ext_len, &l, &capacity)) {
		return l;
	} else {
		if(l.str) {
			vimwiki_free(vw, (void*)l.str);
		}
		memset(&l, 0, sizeof(l));
		return l;
	}
}

size_t vimwiki_query_wiki_paths(vimwiki_path *w, size_t max_w) {
	/* Query vim to echo variable value, e.g.
	 * `vim -E -c 'redi! > /dev/stdout | echo g:vimwiki_list | qa'`
	 */

	// FIXME add support for g:vimwiki_ext2syntax

	char readbuf[4096];
	readbuf[0] = '\0';

	// run vim, query variable value
	FILE *p = popen("vim -E -c 'redi! > /dev/stdout" " | echo g:vimwiki_list | qa'", "r");
	if(!p) {
		fprintf(stderr, "can't launch vim: %s\n", strerror(errno));
		return 0;
	}
	// read stdout
	while(fgets(readbuf, sizeof(readbuf), p)) {}

	pclose(p);

	if(!readbuf[0]) {
		fprintf(stderr, "can't get vimwiki list from vim\n");
		return 0;
	}

	char *vimwiki_list = readbuf;
	size_t vimwiki_list_size = strlen(readbuf);

	// parse json, extract path, ext, syntax
	struct json_value_s *json_root = json_parse_ex(vimwiki_list, vimwiki_list_size,
			json_parse_flags_allow_json5,
			NULL, NULL, NULL);
	if(!json_root) {
		fprintf(stderr, "can't parse JSON with vimwiki list:\n%s\n", vimwiki_list);
		return 0;
	}

	// extract wiki list from JSON
	// TODO error handling
	assert(json_root->type == json_type_array);
	struct json_array_s *json_array = (struct json_array_s*)json_root->payload;

	const size_t wikis_count = json_array->length;
	size_t wiki_i = 0;

	for(struct json_array_element_s *e = json_array->start; e; e = e->next, wiki_i++) {
		const char *path = NULL, *ext = NULL, *syntax = NULL;

		struct json_value_s *obj_value = e->value;
		assert(obj_value->type == json_type_object);
		struct json_object_s *obj = (struct json_object_s*)obj_value->payload;
		for(struct json_object_element_s *oe = obj->start; oe; oe = oe->next) {
			struct json_string_s *name = oe->name;
			if(oe->value->type == json_type_string) {
				struct json_string_s *value = (struct json_string_s*)oe->value->payload;
				//printf("%s => %s\n", name->string, value->string);
				if(strcmp(name->string, "path") == 0) {
					path = value->string;
				} else if(strcmp(name->string, "ext") == 0) {
					ext = value->string;
				} else if(strcmp(name->string, "syntax") == 0) {
					syntax = value->string;
				}
			}
		}

		if(!path || !ext || !syntax) {
			fprintf(stderr, "can't load wiki entry %zd\n", wiki_i);
			continue;
		}
		if(wiki_i >= max_w) {
			continue;
		}

		vimwiki_strlcpy(w[wiki_i].path, path, sizeof(w[wiki_i].path));
		vimwiki_strlcpy(w[wiki_i].ext, ext, sizeof(w[wiki_i].ext));
		w[wiki_i].syntax = vimwiki_syntax_from_str(syntax);
	}
	free(json_root);

	return wikis_count;
}

vimwiki *vimwiki_load(const char *path, const char *ext, enum vimwiki_syntax_e syntax) {
	(void)syntax;

	vimwiki vw = {};
	const size_t wiki_root_size = snprintf(vw.wiki_root, sizeof(vw.wiki_root), "%s", path);
	assert(wiki_root_size < sizeof(vw.wiki_root));
	vw.wiki_root_length = wiki_root_size;
	if(vw.wiki_root_length == 0 || vw.wiki_root[vw.wiki_root_length-1] != '/') {
		vw.wiki_root[vw.wiki_root_length++] = '/';
	}

	// scan wiki directory, get list of all files
	vimwiki_string_list files = vimwiki_get_file_list(&vw, path, ext);
	vw.file_list = files;

	vimwiki *vwp = vimwiki_alloc(&vw, sizeof(*vwp));
	if(!vwp) { return NULL; }
	memcpy(vwp, &vw, sizeof(vw));
	return vwp;
}

void vimwiki_destroy(vimwiki *vw) {
	vimwiki_free(vw, (char*)vw->file_list.str);
	vimwiki_free(vw, vw);
}

// notes parser {{{
static const char *vimwiki_line_end(const char *s) {
	char c;
	while((c = *s++)) {
		if(c == '\n' || c == '\0') {
			return s;
		}
	}
	return s;
}

static bool vimwiki_isspace(char c) {
	return (c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n');
}

static const char *vimwiki_trim_left(const char *str, const char *end) {
	char c = *str;
	while(c && str < end) {
		if(vimwiki_isspace(c)) {
			str++;
			c = *str;
		}
		break;
	}
	return str;
}

// loads timestamp, returns end (right after parsed part)
const char *vimwiki_str_to_timestamp(const char *str, uint64_t *timestamp) {
	// FIXME rewrite without strptime, make format parsing more permissive
	struct tm tm = {};
	const char *end = strptime(str, "%Y-%m-%d %HH:%MM:%SS", &tm);
	if(!end) {
		// retry with just date
		end = strptime(str, "%Y-%m-%d", &tm);
	}

	if(end) {
		*timestamp = timelocal(&tm);
		return end;
	}
	return str;
}

void vimwiki_str_from_timestamp(char *s, size_t sz, uint64_t timestamp) {
	struct tm tm = {};
	time_t t = (time_t)timestamp;
	localtime_r(&t, &tm);
	strftime(s, sz, "%F %T", &tm);
}

static void vimwiki_parse_single_note(vimwiki *vw, vimwiki_note *note,
		const char *note_start, const char *note_end) {
	// fill note data from given text
	const char *s = vimwiki_trim_left(note_start, note_end);
	char c = *s++;

	// skip starting -#*
	if((c == '-' || c == '#' || c == '*') && (s < note_end && *s != c)) {
		s = vimwiki_trim_left(s, note_end);
		c = *s++;
	}

	// check for [ ] part
	if(c == '[' && s < note_end-1) {
		char ibr = s[0];
		if(ibr != ']' && s[1] == ']') {
			if(ibr == 'X') {
				note->completed = true;
			}
			s++;
			c = *s++;
		}
	}

	while(s < note_end) {
		if(c == '@') {
			bool is_scheduled = false, is_deadline = false;
			if(s[0] == '!') {
				s++;
				is_deadline = true;
			} else {
				is_scheduled = true;
			}

			uint64_t timestamp = 0;
			const char *end = vimwiki_str_to_timestamp(s, &timestamp);
			if(end && end != s) {
				if(is_deadline) {
					note->timestamp_deadline = timestamp;
				} else if(is_scheduled) {
					note->timestamp_scheduled = timestamp;
				}

				s = end;
				c = *s;
				continue;
			}
		}
		c = *s++;
	}

	const size_t note_text_length = note_end - note_start;
	if(note_text_length > 1) {
		note->text = vimwiki_alloc(vw, note_text_length+1);
		if(note->text) {
			memcpy(note->text, note_start, note_text_length);
			note->text[note_text_length] = '\0';
		}
	}
}

static bool vimwiki_parse_note(vimwiki *vw,
		const char *local_file_name,
		const char *note_start, const char *note_end,
		vimwiki_note **notes, size_t *num_notes, size_t *notes_capacity) {
	//printf("flush %.*s\n", (int)(note_end - note_start)-1, note_start);
	vimwiki_note note = {};
	vimwiki_parse_single_note(vw, &note, note_start, note_end);
	note.filename = local_file_name;

	// insert
	const size_t current_capacity = *notes_capacity;
	const size_t current_len = *num_notes;
	if(current_len + 1 >= current_capacity) {
		// resize
		const size_t new_capacity = 
				VIMWIKI_MAX(current_capacity*2, VIMWIKI_MIN_ARRAY_CAPACITY);
		vimwiki_note *new_notes = vimwiki_realloc(vw, *notes, new_capacity * sizeof(vimwiki_note));
		if(!new_notes) {
			return false;
		}
		*notes_capacity = new_capacity;
		*notes = new_notes;
	}

	// append
	memcpy((*notes)+current_len, &note, sizeof(note));
	*num_notes = current_len+1;
	return true;
}

static bool vimwiki_parse_notes(vimwiki *vw,
		const char *local_file_name,
		const char *file_contents, size_t file_contents_size,
		vimwiki_note **notes, size_t *num_notes, size_t *notes_capacity) {
	// FIXME add markdown syntax support

	// parsing is line-by-line, stateful

	const char *item_start = NULL;

	const char *const file_end = file_contents + file_contents_size;
	const char *str_end = vimwiki_line_end(file_contents);
	for(const char *str = file_contents; str < file_end;
			str = str_end, str_end = vimwiki_line_end(str)) {
		const char *str_triml = vimwiki_trim_left(str, str_end);
		if(str_triml >= str_end) {
			// empty line is a flush, but no other parsing is necessary
			if(item_start) {
				vimwiki_parse_note(vw, local_file_name, item_start, str, notes, num_notes, notes_capacity);
				item_start = NULL;
			}

			continue;
		}

		bool flush = false;
		const char *new_item_start = NULL;
		const char *s = str_triml;
		char f = *s;
		if((f == '-' || f == '*') && (s[1] == ' ' || s[1] == '\t')) {
			// list item
			new_item_start = str;
			flush = true;
		} else if(f == '#') {
			flush = true;
		}

		if(flush && item_start) {
			// flush
			vimwiki_parse_note(vw, local_file_name, item_start, str, notes, num_notes, notes_capacity);
		}

		if(new_item_start) {
			item_start = new_item_start;
		}
	}
	if(item_start) {
		// flush
		vimwiki_parse_note(vw, local_file_name, item_start, str_end, notes, num_notes, notes_capacity);
	}

	return true;
}

vimwiki_note *vimwiki_load_notes(vimwiki *vw, size_t *out_num_notes, uint32_t flags) {
	(void)flags;

	vimwiki_note *notes = NULL;
	size_t num_notes = 0;
	size_t notes_capacity = 0;

	// get file list, load each file, search for notes, add to array
	for(size_t i = 0, j = 0; i < vw->file_list.num_strings;
			++i, j += strlen(vw->file_list.str+j)+1) {
		const char *local_file_name = vw->file_list.str+j;
		char file_path[VIMWIKI_PATH_MAX];
		const size_t file_path_size = snprintf(file_path, sizeof(file_path), "%s%s",
				vw->wiki_root, vw->file_list.str+j);
		assert(file_path_size < sizeof(file_path));

		//printf("loading file %s\n", file_path);
		size_t file_contents_size = 0;
		char *file_contents = vimwiki_slurp(vw, file_path, &file_contents_size);
		if(!file_contents) {
			fprintf(stderr, "can't load file \"%s\": %s\"\n", file_path, strerror(errno));
			continue;
			// TODO add error reporting
		}

		if(!vimwiki_parse_notes(vw, local_file_name,
					file_contents, file_contents_size,
					&notes, &num_notes, &notes_capacity)) {
			fprintf(stderr, "error parsing notes from file \"%s\"\n", file_path);
		}

		vimwiki_free(vw, file_contents);
	}

	*out_num_notes = num_notes;
	return notes;
}
// }}} notes parser

void vimwiki_destroy_notes(vimwiki *vw, vimwiki_note *notes, size_t num_notes) {
	for(size_t i = 0; i != num_notes; ++i) {
		if(notes[i].text) {
			vimwiki_free(vw, notes[i].text);
		}
	}
	vimwiki_free(vw, notes);
}
