/* Various output modes, colouring, etc.
 */

#include "output.h"
#include "vimwiki.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

void print_init(printer_state *s) {
	s->colour_output = true;
}

void printer_deinit(printer_state *s) {
	(void)s;
}

void print_header(printer_state *s, enum print_section_header_e section, const char *header) {
	(void)s;
#define FBLACK      "\033[30m"
#define FRED        "\033[31m"
#define FGREEN      "\033[32m"
#define FYELLOW     "\033[33m"
#define FBLUE       "\033[34m"
#define FPURPLE     "\033[35m"
#define D_FGREEN    "\033[6m"
#define FWHITE      "\033[7m"
#define FCYAN       "\x1b[36m"

//end color
#define NONE        "\033[0m"

	const char *col_start = NULL;
	switch(section) {
	case PRINT_SECTION_PAST_DEADLINE:
	case PRINT_SECTION_PAST_SCHEDULED:
		col_start = FRED;
		break;
	case PRINT_SECTION_UPCOMING_DEADLINE:
	case PRINT_SECTION_UPCOMING_SCHEDULED:
		col_start = FGREEN;
		break;
	default:
		col_start = NONE;
	}
	printf("%s%s%s\n", col_start, header, NONE);
}

void print_note(printer_state *s, const struct vimwiki_note *note) {
	(void)s;

	char deadline_buf[64], scheduled_buf[64];
	deadline_buf[0] = '\0';
	scheduled_buf[0] = '\0';
	if(note->timestamp_deadline) {
		deadline_buf[0] = '!';
		vimwiki_str_from_timestamp(deadline_buf+1, sizeof(deadline_buf)-1, note->timestamp_deadline);
	}
	if(note->timestamp_scheduled)
		vimwiki_str_from_timestamp(scheduled_buf, sizeof(scheduled_buf), note->timestamp_scheduled);

	const char *gap = (scheduled_buf[0] && deadline_buf[0]) ? " " : "";
	printf(FGREEN "%s%s%s %s\n" NONE, scheduled_buf, gap, deadline_buf, note->filename);
	if(note->text) {
		printf("%s\n", note->text);
	}
}

// print section header and all notes within given range
// timestamp_offset is offset to uint64_t within note (which timestamp to use)
static size_t print_section(printer_state *s, const struct vimwiki_note *notes, size_t num_notes,
		size_t timestamp_offset, uint64_t ts_min, uint64_t ts_max,
		enum print_section_header_e section_header, const char *section_header_text) {
	size_t num_notes_printed = 0;
	for(size_t ni = 0; ni != num_notes; ++ni) {
		const vimwiki_note *n = &notes[ni];
		uint64_t t;
		memcpy(&t, (char*)n+timestamp_offset, sizeof(t));
		if(!n->completed && t && t < ts_max && t >= ts_min) {
			if(num_notes_printed == 0) {
				print_header(s, section_header, section_header_text);
			}

			print_note(s, n);
			num_notes_printed++;
		}
	}

	if(num_notes_printed) {
		printf("\n");
	}

	return num_notes_printed;
}

void print_notes(printer_state *s, const struct vimwiki_note *notes, size_t num_notes,
		uint64_t lookup_timestamp) {
	const uint64_t current_timestamp = time(NULL);

	print_section(s, notes, num_notes,
			offsetof(struct vimwiki_note, timestamp_deadline), 0, current_timestamp,
			PRINT_SECTION_PAST_DEADLINE, "PAST DEADLINE");
	print_section(s, notes, num_notes,
			offsetof(struct vimwiki_note, timestamp_scheduled), 0, current_timestamp,
			PRINT_SECTION_PAST_SCHEDULED, "PAST SCHEDULED");
	print_section(s, notes, num_notes,
			offsetof(struct vimwiki_note, timestamp_deadline), current_timestamp, lookup_timestamp,
			PRINT_SECTION_UPCOMING_DEADLINE, "APPROACHING DEADLINE");
	print_section(s, notes, num_notes,
			offsetof(struct vimwiki_note, timestamp_scheduled), current_timestamp, lookup_timestamp,
			PRINT_SECTION_UPCOMING_SCHEDULED, "UPCOMING");
}
