/* Pretty printing functions
 */

#ifndef _hOUTPUT_H
#define _hOUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct vimwiki_note;

typedef struct printer_state {
	bool colour_output;
} printer_state;

enum print_section_header_e {
	PRINT_SECTION_NONE,
	PRINT_SECTION_PAST_DEADLINE,
	PRINT_SECTION_PAST_SCHEDULED,
	PRINT_SECTION_UPCOMING_DEADLINE,
	PRINT_SECTION_UPCOMING_SCHEDULED,

	PRINT_SECTION_LAST
};

void print_init(printer_state *s);
void printer_deinit(printer_state *s);

void print_header(printer_state *s, enum print_section_header_e section, const char *header);
void print_note(printer_state *s, const struct vimwiki_note *note);

// pretty-print notes; notes should be sorted by date
void print_notes(printer_state *s, const struct vimwiki_note *notes, size_t num_notes,
		uint64_t lookup_timestamp);

#endif
